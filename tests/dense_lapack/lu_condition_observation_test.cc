// POSIX fault signals are used only as terminating child-process outcomes.
#include <signal.h>  // NOLINT(modernize-deprecated-headers)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"
#include "lu_aux_info_test_support.h"

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapack.h>
#include <lapacke_config.h>

namespace asc::internal_gecon_test {
using asc_lu_aux_info_test::Scratch;
using asc_lu_aux_info_test::Take;
using asc_lu_aux_info_test::TestContext;
constexpr auto kHost = MemorySpace::kHost;
constexpr auto kRow = DenseBlasLayout::kRowMajor;
constexpr auto kColumn = DenseBlasLayout::kColumnMajor;
template <typename T>
using Real = DenseBlasRealType<T>;

template <typename T>
class Page {
 public:
  Page() {
    const auto size = sysconf(_SC_PAGESIZE);
    if (size <= 0 || static_cast<std::size_t>(size) % sizeof(T) != 0) {
      std::abort();
    }
    bytes_ = static_cast<std::size_t>(size);
    void* memory = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
      std::abort();
    }
    data_ = new (memory) T[bytes_ / sizeof(T)]{};
  }
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;
  Page(Page&&) = delete;
  Page& operator=(Page&&) = delete;
  ~Page() {
    Open();
    std::destroy_n(data_, bytes_ / sizeof(T));
    if (munmap(data_, bytes_) != 0) {
      std::abort();
    }
  }
  T* data() { return data_; }
  MutableMemoryView storage() { return {data_, bytes_, kHost}; }
  void Protect() {
    if (mprotect(data_, bytes_, PROT_NONE) != 0) {
      std::abort();
    }
  }
  void Open() {
    if (mprotect(data_, bytes_, PROT_READ | PROT_WRITE) != 0) {
      std::abort();
    }
  }

 private:
  T* data_ = nullptr;
  std::size_t bytes_ = 0;
};

template <typename Function>
bool Child(Function function, bool fault = false) {
  const pid_t pid = fork();
  if (pid < 0) {
    std::abort();
  }
  if (pid == 0) {
    ::signal(SIGSEGV, SIG_DFL);
    ::signal(SIGBUS, SIG_DFL);
    std::_Exit(function() ? 0 : 1);
  }
  int status = 0;
  pid_t result = 0;
  do {
    result = waitpid(pid, &status, 0);
  } while (result < 0 && errno == EINTR);
  if (result != pid) {
    std::abort();
  }
  return fault ? WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV
               : WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

thread_local unsigned entries = 0;
thread_local const void* expected_output = nullptr;
thread_local void (*entry_hook)() = nullptr;
template <typename R>
thread_local Page<R>* active_output = nullptr;
template <typename R>
void OpenOutput() {
  active_output<R>->Open();
}

void Enter(const char* norm, const lapack_int* n, const lapack_int* lda,
           const void* output) {
  if ((*norm != '1' && *norm != 'I') || *n != 3 || (*lda != 3 && *lda != 4) ||
      output != expected_output || entry_hook == nullptr) {
    std::abort();
  }
  ++entries;
  entry_hook();
}

template <typename R>
void Calibrate(TestContext& test, Page<R>& output) {
  auto& rcond = output.data()[0];
  ASC_DENSE_TEST_CHECK(test, Child(
                                 [&] {
                                   output.Protect();
                                   volatile R old_output = rcond;
                                   static_cast<void>(old_output);
                                   return true;
                                 },
                                 true));
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         output.Protect();
                         output.Open();
                         rcond = 17;
                         return rcond == 17;
                       }));
}

template <typename T, typename Query, typename Execute>
void Preflight(TestContext& test, Page<T>& input, Page<Real<T>>& output,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, Query query, Execute execute) {
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         input.Protect();
                         output.Protect();
                         return query().ok() && entries == 0;
                       }));
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         auto stale = plan;
                         ++stale.total_byte_limit;
                         input.Protect();
                         output.Protect();
                         return execute(stale, workspace).code() ==
                                    ErrorCode::kInvalidState &&
                                entries == 0;
                       }));
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    if (workspace.regions[i].size() == 0) {
      continue;
    }
    ASC_DENSE_TEST_CHECK(test, Child([&] {
                           auto short_work = workspace;
                           short_work.regions[i] = {
                               workspace.regions[i].data(),
                               workspace.regions[i].size() - 1, kHost};
                           input.Protect();
                           output.Protect();
                           return execute(plan, short_work).code() ==
                                      ErrorCode::kInvalidArgument &&
                                  entries == 0;
                         }));
  }
}

template <typename T>
void Observe(TestContext& test, int n, DenseBlasLayout layout,
             LapackConditionNorm norm) {
  const auto provider =
      Take(ReferenceLapackProvider::Create(ExecutionContext::Serial()));
  Page<T> input;
  Page<Real<T>> output;
  input.data()[0] = T{1};
  input.data()[5] = T{2};
  input.data()[10] = T{4};
  auto& rcond = output.data()[0];
  rcond = -11;
  const auto factors = Take(DenseBlasMatrixView<const T>::Create(
      input.data(), n, n, layout, 4, input.storage()));
  const auto plan =
      Take(QueryGeconWorkspace(provider, norm, factors, Real<T>{4}, rcond));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  active_output<Real<T>> = &output;
  expected_output = &rcond;
  entry_hook = &OpenOutput<Real<T>>;
  const auto execute = [&](const auto& selected, const auto& work) {
    LapackReport report;
    return Gecon(provider, norm, factors, Real<T>{4}, rcond, selected, work,
                 report);
  };
  Calibrate(test, output);
  Preflight(
      test, input, output, plan, workspace,
      [&] {
        return QueryGeconWorkspace(provider, norm, factors, Real<T>{4}, rcond);
      },
      execute);
  if (n == 0) {
    ASC_DENSE_TEST_CHECK(test, Child([&] {
                           input.Protect();
                           return execute(plan, workspace).ok() && rcond == 1 &&
                                  entries == 0;
                         }));
  } else {
    ASC_DENSE_TEST_CHECK(test, Child([&] {
                           output.Protect();
                           return execute(plan, workspace).ok() &&
                                  rcond == Real<T>{0.25} && entries == 1;
                         }));
    if (layout == kRow) {
      ASC_DENSE_TEST_CHECK(test, Child(
                                     [&] {
                                       input.Protect();
                                       return execute(plan, workspace).ok();
                                     },
                                     true));
    }
  }
  entry_hook = nullptr;
  expected_output = nullptr;
  active_output<Real<T>> = nullptr;
}

template <typename T>
void Run(TestContext& test) {
  for (int n : {0, 3}) {
    for (auto layout : {kRow, kColumn}) {
      for (auto norm :
           {LapackConditionNorm::kOne, LapackConditionNorm::kInfinity}) {
        Observe<T>(test, n, layout, norm);
      }
    }
  }
}
}  // namespace asc::internal_gecon_test

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  asc::internal_gecon_test::TestContext test;
  asc::internal_gecon_test::Run<float>(test);
  asc::internal_gecon_test::Run<double>(test);
  asc::internal_gecon_test::Run<std::complex<float>>(test);
  asc::internal_gecon_test::Run<std::complex<double>>(test);
  std::printf(
      "GECON observer:32 scalar/norm/layout/path cases,208 controls; "
      "32 negative reads,32 restored writes,32 queries,32 stale plans,"
      "40 short workspaces,16 local completions,16 validated native "
      "entries,8 legitimate row-packing reads. ASC pre-native scope; "
      "local outputs writable, local no-read semantics source-reviewed.\n");
  return test.Finish();
}

// GNU ld requires reserved spellings for test-only foreign interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_sgecon_(const char* norm, const lapack_int* n,
                               const float* a, const lapack_int* lda,
                               const float* anorm, float* rcond, float* work,
                               lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_sgecon_(const char* norm, const lapack_int* n,
                               const float* a, const lapack_int* lda,
                               const float* anorm, float* rcond, float* work,
                               lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  asc::internal_gecon_test::Enter(norm, n, lda, rcond);
  __real_sgecon_(norm, n, a, lda, anorm, rcond, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_sgecon_), decltype(&LAPACK_sgecon_base)>);
// NOLINTEND(bugprone-reserved-identifier)

// GNU ld requires reserved spellings for test-only foreign interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_dgecon_(const char* norm, const lapack_int* n,
                               const double* a, const lapack_int* lda,
                               const double* anorm, double* rcond, double* work,
                               lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_dgecon_(const char* norm, const lapack_int* n,
                               const double* a, const lapack_int* lda,
                               const double* anorm, double* rcond, double* work,
                               lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  asc::internal_gecon_test::Enter(norm, n, lda, rcond);
  __real_dgecon_(norm, n, a, lda, anorm, rcond, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_dgecon_), decltype(&LAPACK_dgecon_base)>);
// NOLINTEND(bugprone-reserved-identifier)

// GNU ld requires reserved spellings for test-only foreign interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_cgecon_(const char* norm, const lapack_int* n,
                               const std::complex<float>* a,
                               const lapack_int* lda, const float* anorm,
                               float* rcond, std::complex<float>* work,
                               float* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_cgecon_(const char* norm, const lapack_int* n,
                               const std::complex<float>* a,
                               const lapack_int* lda, const float* anorm,
                               float* rcond, std::complex<float>* work,
                               float* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  asc::internal_gecon_test::Enter(norm, n, lda, rcond);
  __real_cgecon_(norm, n, a, lda, anorm, rcond, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_cgecon_), decltype(&LAPACK_cgecon_base)>);
// NOLINTEND(bugprone-reserved-identifier)

// GNU ld requires reserved spellings for test-only foreign interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_zgecon_(const char* norm, const lapack_int* n,
                               const std::complex<double>* a,
                               const lapack_int* lda, const double* anorm,
                               double* rcond, std::complex<double>* work,
                               double* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_zgecon_(const char* norm, const lapack_int* n,
                               const std::complex<double>* a,
                               const lapack_int* lda, const double* anorm,
                               double* rcond, std::complex<double>* work,
                               double* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (length != 1) {
    std::abort();
  }
#endif
  asc::internal_gecon_test::Enter(norm, n, lda, rcond);
  __real_zgecon_(norm, n, a, lda, anorm, rcond, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_zgecon_), decltype(&LAPACK_zgecon_base)>);
// NOLINTEND(bugprone-reserved-identifier)
