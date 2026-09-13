// POSIX signals are terminating child outcomes, not recovery handlers.
#include <signal.h>  // NOLINT(modernize-deprecated-headers)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
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
#include "asc/dense/providers/lapack_lu_equilibration.h"
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

namespace asc::internal_geequ_test {
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
    if (size <= 0 || static_cast<std::size_t>(size) < sizeof(T)) {
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
thread_local std::array<const void*, 5> expected_outputs{};
thread_local bool expected_radix = false;
thread_local void (*entry_hook)() = nullptr;

template <typename R>
struct Outputs {
  Page<R> rows;
  Page<R> columns;
  Page<LapackEquilibrationStatistics<R>> statistics;
  void Protect() {
    rows.Protect();
    columns.Protect();
    statistics.Protect();
  }
  void Open() {
    rows.Open();
    columns.Open();
    statistics.Open();
  }
  auto Addresses() {
    auto& stats = statistics.data()[0];
    return std::array<R*, 5>{rows.data(), columns.data(), &stats.row_condition,
                             &stats.column_condition, &stats.absolute_maximum};
  }
};
template <typename R>
thread_local Outputs<R>* active_outputs = nullptr;
template <typename R>
void OpenOutputs() {
  active_outputs<R>->Open();
}

void Enter(bool radix, const lapack_int* m, const lapack_int* n,
           const lapack_int* lda, std::array<const void*, 5> outputs) {
  if (radix != expected_radix || *m != 3 || *n != 2 ||
      (*lda != 3 && *lda != 4) || outputs != expected_outputs ||
      entry_hook == nullptr) {
    std::abort();
  }
  ++entries;
  entry_hook();
}

template <typename R>
void Calibrate(TestContext& test, Outputs<R>& output) {
  // Every pointer names an initialized live object, including each statistics
  // member. The statistics page protects all three members together.
  const auto addresses = output.Addresses();
  for (const auto* address : addresses) {
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     output.Protect();
                                     volatile R old_output = *address;
                                     static_cast<void>(old_output);
                                     return true;
                                   },
                                   true));
  }
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         output.Protect();
                         output.Open();
                         for (auto* address : addresses) {
                           *address = 17;
                         }
                         return output.statistics.data()[0].absolute_maximum ==
                                17;
                       }));
}

template <typename T, typename Query, typename Execute>
void Preflight(TestContext& test, Page<T>& input, Outputs<Real<T>>& outputs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, Query query, Execute execute) {
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         input.Protect();
                         outputs.Protect();
                         return query().ok() && entries == 0;
                       }));
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         auto stale = plan;
                         stale.total_byte_limit = 0;
                         input.Protect();
                         outputs.Protect();
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
                           outputs.Protect();
                           return execute(plan, short_work).code() ==
                                      ErrorCode::kInvalidArgument &&
                                  entries == 0;
                         }));
  }
}

template <typename T>
void Observe(TestContext& test, int m, int n, DenseBlasLayout layout,
             bool radix) {
  const auto provider =
      Take(ReferenceLapackProvider::Create(ExecutionContext::Serial()));
  Page<T> input;
  Outputs<Real<T>> outputs;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      input.data()[layout == kRow ? 4 * i + j : 4 * j + i] = T{1};
    }
  }
  outputs.rows.data()[0] = -11;
  outputs.columns.data()[0] = -13;
  auto& stats = outputs.statistics.data()[0];
  stats = {-17, -19, -23};
  const auto matrix = Take(DenseBlasMatrixView<const T>::Create(
      input.data(), m, n, layout, 4, input.storage()));
  const auto rows = Take(DenseBlasVectorView<Real<T>>::Create(
      outputs.rows.data(), m, 1, outputs.rows.storage()));
  const auto columns = Take(DenseBlasVectorView<Real<T>>::Create(
      outputs.columns.data(), n, 1, outputs.columns.storage()));
  const auto query = [&] {
    return radix ? QueryGeequbWorkspace(provider, matrix, rows, columns, stats)
                 : QueryGeequWorkspace(provider, matrix, rows, columns, stats);
  };
  const auto plan = Take(query());
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  expected_radix = radix;
  const auto addresses = outputs.Addresses();
  for (std::size_t i = 0; i < addresses.size(); ++i) {
    expected_outputs[i] = addresses[i];
  }
  active_outputs<Real<T>> = &outputs;
  entry_hook = &OpenOutputs<Real<T>>;
  const auto execute = [&](const auto& selected, const auto& work) {
    LapackReport report;
    return radix ? Geequb(provider, matrix, rows, columns, stats, selected,
                          work, report)
                 : Geequ(provider, matrix, rows, columns, stats, selected, work,
                         report);
  };
  Calibrate(test, outputs);
  Preflight(test, input, outputs, plan, workspace, query, execute);
  if (m == 0 || n == 0) {
    ASC_DENSE_TEST_CHECK(test, Child([&] {
                           input.Protect();
                           outputs.rows.Protect();
                           outputs.columns.Protect();
                           return execute(plan, workspace).ok() &&
                                  entries == 0 && stats.row_condition == 1 &&
                                  stats.column_condition == 1 &&
                                  stats.absolute_maximum == 0;
                         }));
  } else {
    ASC_DENSE_TEST_CHECK(test, Child([&] {
                           outputs.Protect();
                           return execute(plan, workspace).ok() &&
                                  entries == 1 && outputs.rows.data()[0] == 1 &&
                                  outputs.columns.data()[0] == 1 &&
                                  stats.row_condition == 1 &&
                                  stats.column_condition == 1 &&
                                  stats.absolute_maximum == 1;
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
  active_outputs<Real<T>> = nullptr;
  expected_outputs = {};
}

template <typename T>
void Run(TestContext& test) {
  for (const auto shape : {std::array{0, 0}, std::array{0, 2}, std::array{3, 0},
                           std::array{3, 2}}) {
    for (auto layout : {kRow, kColumn}) {
      for (bool radix : {false, true}) {
        Observe<T>(test, shape[0], shape[1], layout, radix);
      }
    }
  }
}
}  // namespace asc::internal_geequ_test

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  asc::internal_geequ_test::TestContext test;
  asc::internal_geequ_test::Run<float>(test);
  asc::internal_geequ_test::Run<double>(test);
  asc::internal_geequ_test::Run<std::complex<float>>(test);
  asc::internal_geequ_test::Run<std::complex<double>>(test);
  std::printf(
      "GEEQU/GEEQUB observer:64 scalar/routine/layout/shape cases,"
      "592 controls;320 negative output reads,64 restored writes,"
      "64 queries,64 stale plans,8 short workspaces,48 local "
      "completions,16 validated native entries,8 legitimate row-pack "
      "reads. ASC pre-native scope; local statistics writable and "
      "their no-read completion source-reviewed.\n");
  return test.Finish();
}

// Reserved foreign names are required only for GNU test interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_sgeequ_(const lapack_int* m, const lapack_int* n,
                               const float* a, const lapack_int* lda,
                               float* rows, float* columns,
                               float* row_condition, float* column_condition,
                               float* maximum, lapack_int* info);
extern "C" void __wrap_sgeequ_(const lapack_int* m, const lapack_int* n,
                               const float* a, const lapack_int* lda,
                               float* rows, float* columns,
                               float* row_condition, float* column_condition,
                               float* maximum, lapack_int* info) {
  asc::internal_geequ_test::Enter(
      false, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_sgeequ_(m, n, a, lda, rows, columns, row_condition, column_condition,
                 maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_sgeequ_), decltype(&LAPACK_sgeequ)>);

extern "C" void __real_sgeequb_(const lapack_int* m, const lapack_int* n,
                                const float* a, const lapack_int* lda,
                                float* rows, float* columns,
                                float* row_condition, float* column_condition,
                                float* maximum, lapack_int* info);
extern "C" void __wrap_sgeequb_(const lapack_int* m, const lapack_int* n,
                                const float* a, const lapack_int* lda,
                                float* rows, float* columns,
                                float* row_condition, float* column_condition,
                                float* maximum, lapack_int* info) {
  asc::internal_geequ_test::Enter(
      true, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_sgeequb_(m, n, a, lda, rows, columns, row_condition, column_condition,
                  maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_sgeequb_), decltype(&LAPACK_sgeequb)>);

extern "C" void __real_dgeequ_(const lapack_int* m, const lapack_int* n,
                               const double* a, const lapack_int* lda,
                               double* rows, double* columns,
                               double* row_condition, double* column_condition,
                               double* maximum, lapack_int* info);
extern "C" void __wrap_dgeequ_(const lapack_int* m, const lapack_int* n,
                               const double* a, const lapack_int* lda,
                               double* rows, double* columns,
                               double* row_condition, double* column_condition,
                               double* maximum, lapack_int* info) {
  asc::internal_geequ_test::Enter(
      false, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_dgeequ_(m, n, a, lda, rows, columns, row_condition, column_condition,
                 maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_dgeequ_), decltype(&LAPACK_dgeequ)>);

extern "C" void __real_dgeequb_(const lapack_int* m, const lapack_int* n,
                                const double* a, const lapack_int* lda,
                                double* rows, double* columns,
                                double* row_condition, double* column_condition,
                                double* maximum, lapack_int* info);
extern "C" void __wrap_dgeequb_(const lapack_int* m, const lapack_int* n,
                                const double* a, const lapack_int* lda,
                                double* rows, double* columns,
                                double* row_condition, double* column_condition,
                                double* maximum, lapack_int* info) {
  asc::internal_geequ_test::Enter(
      true, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_dgeequb_(m, n, a, lda, rows, columns, row_condition, column_condition,
                  maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_dgeequb_), decltype(&LAPACK_dgeequb)>);

extern "C" void __real_cgeequ_(const lapack_int* m, const lapack_int* n,
                               const std::complex<float>* a,
                               const lapack_int* lda, float* rows,
                               float* columns, float* row_condition,
                               float* column_condition, float* maximum,
                               lapack_int* info);
extern "C" void __wrap_cgeequ_(const lapack_int* m, const lapack_int* n,
                               const std::complex<float>* a,
                               const lapack_int* lda, float* rows,
                               float* columns, float* row_condition,
                               float* column_condition, float* maximum,
                               lapack_int* info) {
  asc::internal_geequ_test::Enter(
      false, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_cgeequ_(m, n, a, lda, rows, columns, row_condition, column_condition,
                 maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_cgeequ_), decltype(&LAPACK_cgeequ)>);

extern "C" void __real_cgeequb_(const lapack_int* m, const lapack_int* n,
                                const std::complex<float>* a,
                                const lapack_int* lda, float* rows,
                                float* columns, float* row_condition,
                                float* column_condition, float* maximum,
                                lapack_int* info);
extern "C" void __wrap_cgeequb_(const lapack_int* m, const lapack_int* n,
                                const std::complex<float>* a,
                                const lapack_int* lda, float* rows,
                                float* columns, float* row_condition,
                                float* column_condition, float* maximum,
                                lapack_int* info) {
  asc::internal_geequ_test::Enter(
      true, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_cgeequb_(m, n, a, lda, rows, columns, row_condition, column_condition,
                  maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_cgeequb_), decltype(&LAPACK_cgeequb)>);

extern "C" void __real_zgeequ_(const lapack_int* m, const lapack_int* n,
                               const std::complex<double>* a,
                               const lapack_int* lda, double* rows,
                               double* columns, double* row_condition,
                               double* column_condition, double* maximum,
                               lapack_int* info);
extern "C" void __wrap_zgeequ_(const lapack_int* m, const lapack_int* n,
                               const std::complex<double>* a,
                               const lapack_int* lda, double* rows,
                               double* columns, double* row_condition,
                               double* column_condition, double* maximum,
                               lapack_int* info) {
  asc::internal_geequ_test::Enter(
      false, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_zgeequ_(m, n, a, lda, rows, columns, row_condition, column_condition,
                 maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_zgeequ_), decltype(&LAPACK_zgeequ)>);

extern "C" void __real_zgeequb_(const lapack_int* m, const lapack_int* n,
                                const std::complex<double>* a,
                                const lapack_int* lda, double* rows,
                                double* columns, double* row_condition,
                                double* column_condition, double* maximum,
                                lapack_int* info);
extern "C" void __wrap_zgeequb_(const lapack_int* m, const lapack_int* n,
                                const std::complex<double>* a,
                                const lapack_int* lda, double* rows,
                                double* columns, double* row_condition,
                                double* column_condition, double* maximum,
                                lapack_int* info) {
  asc::internal_geequ_test::Enter(
      true, m, n, lda,
      {rows, columns, row_condition, column_condition, maximum});
  __real_zgeequb_(m, n, a, lda, rows, columns, row_condition, column_condition,
                  maximum, info);
}
static_assert(
    std::is_same_v<decltype(&__wrap_zgeequb_), decltype(&LAPACK_zgeequb)>);

// NOLINTEND(bugprone-reserved-identifier)
