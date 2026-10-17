// Fault signals terminate calibrated children; no fault handler recovers.
#include <signal.h>  // NOLINT(modernize-deprecated-headers)
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_refinement.h"
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

namespace asc::internal_gerfs_test {
using asc_lu_aux_info_test::Scratch;
using asc_lu_aux_info_test::Take;
using asc_lu_aux_info_test::TestContext;
constexpr auto kHost = MemorySpace::kHost;
constexpr auto kRow = DenseBlasLayout::kRowMajor;
constexpr auto kColumn = DenseBlasLayout::kColumnMajor;
constexpr auto kNone = DenseBlasTranspose::kNone;
constexpr auto kTranspose = DenseBlasTranspose::kTranspose;
constexpr auto kConjugate = DenseBlasTranspose::kConjugateTranspose;
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
thread_local std::array<const void*, 2> expected_outputs{};
thread_local char expected_transpose = 'N';
thread_local void (*entry_hook)() = nullptr;

template <typename T>
struct Frame {
  int n;
  int nrhs;
  unsigned int layouts;
  std::array<Page<T>, 4> matrices;
  Page<index_t> pivots;
  Page<Real<T>> ferr;
  Page<Real<T>> berr;

  Frame(int order, int count, unsigned int mask)
      : n(order), nrhs(count), layouts(mask) {
    for (int i = 0; i < n; ++i) {
      pivots.data()[i] = i + 1;
      const Real<T> diagonal = std::ldexp(Real<T>{1}, i);
      matrices[0].data()[Offset(0, i, i)] = T{diagonal};
      matrices[1].data()[Offset(1, i, i)] = T{diagonal};
      for (int j = 0; j < nrhs; ++j) {
        const auto value = static_cast<Real<T>>(j + 1);
        matrices[2].data()[Offset(2, i, j)] = T{diagonal * value};
        matrices[3].data()[Offset(3, i, j)] = T{Real<T>{0.75} * value};
      }
    }
    for (int j = 0; j < 2; ++j) {
      ferr.data()[j] = -23;
      berr.data()[j] = -29;
    }
  }
  [[nodiscard]] DenseBlasLayout Layout(unsigned int index) const {
    return (layouts & (1U << index)) == 0 ? kColumn : kRow;
  }
  [[nodiscard]] int Offset(unsigned int index, int i, int j) const {
    return Layout(index) == kRow ? 4 * i + j : 4 * j + i;
  }
  auto Input(unsigned int index) {
    return Take(DenseBlasMatrixView<const T>::Create(
        matrices[index].data(), n, index < 2 ? n : nrhs, Layout(index), 4,
        matrices[index].storage()));
  }
  auto Solution() {
    return Take(DenseBlasMatrixView<T>::Create(
        matrices[3].data(), n, nrhs, Layout(3), 4, matrices[3].storage()));
  }
  auto Pivots() {
    return Take(RawLapackPivotView::Create(pivots.data(), n,
                                           LapackFactorFamily::kLuPartialPivot,
                                           pivots.storage()));
  }
  auto Output(Page<Real<T>>& page) {
    return Take(DenseBlasVectorView<Real<T>>::Create(page.data(), nrhs, 1,
                                                     page.storage()));
  }
  auto Query(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose) {
    return QueryGerfsWorkspace(provider, transpose, Input(0), Input(1),
                               Pivots(), Input(2), Solution(), Output(ferr),
                               Output(berr));
  }
  auto Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose transpose, const LapackWorkspacePlan& plan,
               const LapackWorkspace& work, LapackReport& report) {
    return Gerfs(provider, transpose, Input(0), Input(1), Pivots(), Input(2),
                 Solution(), Output(ferr), Output(berr), plan, work, report);
  }
  void ProtectMatrices() {
    for (auto& matrix : matrices) {
      matrix.Protect();
    }
  }
  void ProtectOutputs() {
    ferr.Protect();
    berr.Protect();
  }
  void OpenOutputs() {
    ferr.Open();
    berr.Open();
  }
};

template <typename T>
thread_local Frame<T>* active_frame = nullptr;
template <typename T>
void OpenOutputs() {
  active_frame<T>->OpenOutputs();
}

void Enter(const char* trans, const lapack_int* n, const lapack_int* nrhs,
           std::array<lapack_int, 4> dimensions,
           std::array<const void*, 2> outputs) {
  if (*trans != expected_transpose || *n != 3 || *nrhs != 2 ||
      outputs != expected_outputs || entry_hook == nullptr) {
    std::abort();
  }
  for (auto ld : dimensions) {
    if (ld != 3 && ld != 4) {
      std::abort();
    }
  }
  ++entries;
  entry_hook();
}

enum class Phase : std::uint8_t { kQuery, kStale, kShort, kNative, kLocal };

template <typename T>
unsigned Preflight(TestContext& test, Frame<T>& frame,
                   const ReferenceLapackProvider& provider,
                   DenseBlasTranspose transpose, Phase phase,
                   const LapackWorkspacePlan& plan,
                   const LapackWorkspace& workspace) {
  frame.ProtectMatrices();
  frame.ProtectOutputs();
  // Pivot values remain readable: the accepted query validates them.
  LapackReport report;
  if (phase == Phase::kStale) {
    auto stale = plan;
    stale.total_byte_limit = 0;
    ASC_DENSE_TEST_EQ(
        test,
        frame.Execute(provider, transpose, stale, workspace, report).code(),
        ErrorCode::kInvalidState);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    return 1;
  }
  unsigned checks = 0;
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    if (workspace.regions[i].size() == 0) {
      continue;
    }
    auto short_work = workspace;
    short_work.regions[i] = {workspace.regions[i].data(),
                             workspace.regions[i].size() - 1, kHost};
    ASC_DENSE_TEST_EQ(
        test,
        frame.Execute(provider, transpose, plan, short_work, report).code(),
        ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ++checks;
  }
  return checks;
}

template <typename T>
void ExecuteObserved(TestContext& test, Frame<T>& frame,
                     const ReferenceLapackProvider& provider,
                     DenseBlasTranspose transpose, Phase phase,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace) {
  if (phase == Phase::kLocal) {
    frame.ProtectMatrices();
  } else {
    expected_outputs = {frame.ferr.data(), frame.berr.data()};
    expected_transpose = 'N';
    if (transpose == kTranspose) {
      expected_transpose = 'T';
    } else if (transpose == kConjugate) {
      expected_transpose = 'C';
    }
    active_frame<T> = &frame;
    entry_hook = &OpenOutputs<T>;
    frame.ProtectOutputs();
  }
  LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, frame.Execute(provider, transpose, plan, workspace, report).ok());
  if (phase == Phase::kLocal) {
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, entries, 0U);
    for (int j = 0; j < frame.nrhs; ++j) {
      ASC_DENSE_TEST_EQ(test, frame.ferr.data()[j], Real<T>{0});
      ASC_DENSE_TEST_EQ(test, frame.berr.data()[j], Real<T>{0});
    }
  } else {
    ASC_DENSE_TEST_EQ(test, entries, 1U);
    ASC_DENSE_TEST_EQ(test, report.native_info, 0);
    for (int j = 0; j < frame.nrhs; ++j) {
      for (int i = 0; i < frame.n; ++i) {
        ASC_DENSE_TEST_EQ(test, frame.matrices[3].data()[frame.Offset(3, i, j)],
                          T{static_cast<Real<T>>(j + 1)});
      }
      ASC_DENSE_TEST_CHECK(test, std::isfinite(frame.ferr.data()[j]) &&
                                     frame.ferr.data()[j] >= 0);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(frame.berr.data()[j]) &&
                                     frame.berr.data()[j] >= 0);
    }
  }
  entry_hook = nullptr;
  active_frame<T> = nullptr;
  expected_outputs = {};
}

template <typename T>
unsigned ObserveCase(TestContext& test, Frame<T>& frame,
                     const ReferenceLapackProvider& provider,
                     DenseBlasTranspose transpose, Phase phase) {
  entries = 0;
  if (phase == Phase::kQuery) {
    frame.ProtectMatrices();
    frame.ProtectOutputs();
    ASC_DENSE_TEST_CHECK(test, frame.Query(provider, transpose).ok());
    ASC_DENSE_TEST_EQ(test, entries, 0U);
    return 1;
  }
  const auto plan = Take(frame.Query(provider, transpose));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto before = scratch;
  unsigned checks = 1;
  if (phase == Phase::kStale || phase == Phase::kShort) {
    checks =
        Preflight(test, frame, provider, transpose, phase, plan, workspace);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.real, before.real);
    ASC_DENSE_TEST_EQ(test, scratch.packing, before.packing);
    ASC_DENSE_TEST_EQ(test, scratch.integers, before.integers);
    ASC_DENSE_TEST_EQ(test, entries, 0U);
  } else {
    ExecuteObserved(test, frame, provider, transpose, phase, plan, workspace);
    scratch.Guards(test);
  }
  return checks;
}

template <typename T>
bool ObservePhase(Phase phase) {
  TestContext test;
  const auto provider =
      Take(ReferenceLapackProvider::Create(ExecutionContext::Serial()));
  unsigned cases = 0;
  unsigned checks = 0;
  for (auto transpose : {kNone, kTranspose, kConjugate}) {
    for (unsigned int layouts = 0; layouts < 16; ++layouts) {
      for (auto shape : {std::array{0, 0}, std::array{0, 2}, std::array{3, 0},
                         std::array{3, 2}}) {
        const bool local = shape[0] == 0 || shape[1] == 0;
        if ((phase == Phase::kNative && local) ||
            (phase == Phase::kLocal && !local)) {
          continue;
        }
        Frame<T> frame(shape[0], shape[1], layouts);
        checks += ObserveCase(test, frame, provider, transpose, phase);
        ++cases;
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases,
                    phase == Phase::kNative  ? 48U
                    : phase == Phase::kLocal ? 144U
                                             : 192U);
  std::fprintf(stderr, "GERFS observer bytes=%zu phase=%d cases=%u checks=%u\n",
               sizeof(T), static_cast<int>(phase), cases, checks);
  return test.Finish() == 0;
}

template <typename T>
void Calibrate(TestContext& test) {
  Frame<T> frame(3, 2, 8);  // X row packing; other matrices direct.
  for (auto* output : {frame.ferr.data(), frame.berr.data()}) {
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     frame.ProtectOutputs();
                                     volatile Real<T> old_output = *output;
                                     static_cast<void>(old_output);
                                     return true;
                                   },
                                   true));
  }
  ASC_DENSE_TEST_CHECK(test, Child([&] {
                         frame.ProtectOutputs();
                         frame.OpenOutputs();
                         frame.ferr.data()[0] = 17;
                         frame.berr.data()[0] = 19;
                         return frame.ferr.data()[0] == 17 &&
                                frame.berr.data()[0] == 19;
                       }));
  const auto provider =
      Take(ReferenceLapackProvider::Create(ExecutionContext::Serial()));
  const auto plan = Take(frame.Query(provider, kNone));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  for (unsigned int input : {1U, 3U}) {
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     frame.matrices[input].Protect();
                                     LapackReport report;
                                     return frame
                                         .Execute(provider, kNone, plan,
                                                  workspace, report)
                                         .ok();
                                   },
                                   true));
  }
}

template <typename T>
void Run(TestContext& test) {
  Calibrate<T>(test);
  // One child per phase exercises every case; a single forbidden access fails
  // its entire phase. This retains all modes without a fork for every query.
  for (auto phase : {Phase::kQuery, Phase::kStale, Phase::kShort,
                     Phase::kNative, Phase::kLocal}) {
    ASC_DENSE_TEST_CHECK(test, Child([&] { return ObservePhase<T>(phase); }));
  }
}
}  // namespace asc::internal_gerfs_test

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  asc::internal_gerfs_test::TestContext test;
  asc::internal_gerfs_test::Run<float>(test);
  asc::internal_gerfs_test::Run<double>(test);
  asc::internal_gerfs_test::Run<std::complex<float>>(test);
  asc::internal_gerfs_test::Run<std::complex<double>>(test);
  std::printf(
      "GERFS observer:40 calibrated/phase children;8 forbidden output "
      "reads,4 restored writes,8 legitimate U/X reads. Allfour "
      "scalars,3 transposes,16 independent layouts,4 shape paths; "
      "phase logs record actual cases/checks. ASC pre-native scope; "
      "local estimates writable, their no-read completion reviewed "
      "separately; native-return estimate scans permitted.\n");
  return test.Finish();
}

// Reserved spellings are required only for GNU foreign-entry interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_sgerfs_(const char* trans, const lapack_int* n,
                               const lapack_int* nrhs, const float* a,
                               const lapack_int* lda, const float* af,
                               const lapack_int* ldaf, const lapack_int* pivots,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               float* work, lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_sgerfs_(const char* trans, const lapack_int* n,
                               const lapack_int* nrhs, const float* a,
                               const lapack_int* lda, const float* af,
                               const lapack_int* ldaf, const lapack_int* pivots,
                               const float* b, const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               float* work, lapack_int* extra, lapack_int* info
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
  asc::internal_gerfs_test::Enter(trans, n, nrhs, {*lda, *ldaf, *ldb, *ldx},
                                  {ferr, berr});
  __real_sgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_sgerfs_), decltype(&LAPACK_sgerfs_base)>);

extern "C" void __real_dgerfs_(const char* trans, const lapack_int* n,
                               const lapack_int* nrhs, const double* a,
                               const lapack_int* lda, const double* af,
                               const lapack_int* ldaf, const lapack_int* pivots,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* ferr,
                               double* berr, double* work, lapack_int* extra,
                               lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
                               ,
                               FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_dgerfs_(const char* trans, const lapack_int* n,
                               const lapack_int* nrhs, const double* a,
                               const lapack_int* lda, const double* af,
                               const lapack_int* ldaf, const lapack_int* pivots,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* ferr,
                               double* berr, double* work, lapack_int* extra,
                               lapack_int* info
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
  asc::internal_gerfs_test::Enter(trans, n, nrhs, {*lda, *ldaf, *ldb, *ldx},
                                  {ferr, berr});
  __real_dgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_dgerfs_), decltype(&LAPACK_dgerfs_base)>);

extern "C" void __real_cgerfs_(
    const char* trans, const lapack_int* n, const lapack_int* nrhs,
    const std::complex<float>* a, const lapack_int* lda,
    const std::complex<float>* af, const lapack_int* ldaf,
    const lapack_int* pivots, const std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* x, const lapack_int* ldx,
    float* ferr, float* berr, std::complex<float>* work, float* extra,
    lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_cgerfs_(
    const char* trans, const lapack_int* n, const lapack_int* nrhs,
    const std::complex<float>* a, const lapack_int* lda,
    const std::complex<float>* af, const lapack_int* ldaf,
    const lapack_int* pivots, const std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* x, const lapack_int* ldx,
    float* ferr, float* berr, std::complex<float>* work, float* extra,
    lapack_int* info
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
  asc::internal_gerfs_test::Enter(trans, n, nrhs, {*lda, *ldaf, *ldb, *ldx},
                                  {ferr, berr});
  __real_cgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_cgerfs_), decltype(&LAPACK_cgerfs_base)>);

extern "C" void __real_zgerfs_(
    const char* trans, const lapack_int* n, const lapack_int* nrhs,
    const std::complex<double>* a, const lapack_int* lda,
    const std::complex<double>* af, const lapack_int* ldaf,
    const lapack_int* pivots, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* ferr, double* berr, std::complex<double>* work, double* extra,
    lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN length
#endif
);
extern "C" void __wrap_zgerfs_(
    const char* trans, const lapack_int* n, const lapack_int* nrhs,
    const std::complex<double>* a, const lapack_int* lda,
    const std::complex<double>* af, const lapack_int* ldaf,
    const lapack_int* pivots, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* ferr, double* berr, std::complex<double>* work, double* extra,
    lapack_int* info
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
  asc::internal_gerfs_test::Enter(trans, n, nrhs, {*lda, *ldaf, *ldb, *ldx},
                                  {ferr, berr});
  __real_zgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_zgerfs_), decltype(&LAPACK_zgerfs_base)>);

// NOLINTEND(bugprone-reserved-identifier)
