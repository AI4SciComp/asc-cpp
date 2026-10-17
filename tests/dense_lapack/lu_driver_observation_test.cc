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
#include "asc/dense/providers/lapack_lu_driver.h"
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

namespace asc::internal_gesvx_test {
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

struct Mode {
  char fact;
  LapackEquilibration equilibration;
};
constexpr std::array<Mode, 6> kModes{{
    {'N', LapackEquilibration::kNone},
    {'E', LapackEquilibration::kNone},
    {'F', LapackEquilibration::kNone},
    {'F', LapackEquilibration::kRows},
    {'F', LapackEquilibration::kColumns},
    {'F', LapackEquilibration::kBoth},
}};

[[nodiscard]] bool Rows(Mode mode) {
  return mode.equilibration == LapackEquilibration::kRows ||
         mode.equilibration == LapackEquilibration::kBoth;
}
[[nodiscard]] bool Columns(Mode mode) {
  return mode.equilibration == LapackEquilibration::kColumns ||
         mode.equilibration == LapackEquilibration::kBoth;
}
[[nodiscard]] char Equed(Mode mode) {
  if (Rows(mode)) {
    return Columns(mode) ? 'B' : 'R';
  }
  return Columns(mode) ? 'C' : 'N';
}

struct Boundary {
  char fact = 'N';
  char transpose = 'N';
  char equilibration = 'N';
  lapack_int nrhs = 0;
  std::array<lapack_int, 4> dimensions{};
  std::array<const void*, 4> matrices{};
  std::array<const void*, 5> reals{};
  const void* pivots = nullptr;
};
thread_local Boundary expected;
thread_local unsigned entries = 0;
thread_local void (*entry_hook)() = nullptr;

template <typename T>
struct Frame {
  int n;
  int nrhs;
  unsigned int layouts;
  Mode mode;
  std::array<Page<T>, 4> matrices;
  Page<index_t> pivots;
  Page<Real<T>> rows;
  Page<Real<T>> columns;
  Page<Real<T>> ferr;
  Page<Real<T>> berr;
  Page<LapackSolveStatistics<Real<T>>> statistics;
  Page<LapackEquilibration> equilibration;

  Frame(int order, int count, unsigned int mask, Mode selection)
      : n(order), nrhs(count), layouts(mask), mode(selection) {
    *equilibration.data() =
        mode.fact == 'E' ? LapackEquilibration::kBoth : mode.equilibration;
    *statistics.data() = {-31, -37};
    for (int i = 0; i < n; ++i) {
      const Real<T> diagonal = std::ldexp(Real<T>{1}, i);
      const Real<T> row = Rows(mode) ? std::ldexp(Real<T>{1}, i + 1) : 1;
      const Real<T> column = Columns(mode) ? std::ldexp(Real<T>{1}, -i - 1) : 1;
      pivots.data()[i] = mode.fact == 'F' ? i + 1 : -41;
      rows.data()[i] = mode.fact == 'F' ? row : -43;
      columns.data()[i] = mode.fact == 'F' ? column : -47;
      matrices[0].data()[Offset(0, i, i)] = T{row * diagonal * column};
      matrices[1].data()[Offset(1, i, i)] =
          mode.fact == 'F' ? T{row * diagonal * column} : T{-53};
      for (int j = 0; j < nrhs; ++j) {
        const auto value = static_cast<Real<T>>(j + 1);
        matrices[2].data()[Offset(2, i, j)] = T{diagonal * value};
        matrices[3].data()[Offset(3, i, j)] = T{-59};
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
  auto Matrix(unsigned int index) {
    return Take(DenseBlasMatrixView<T>::Create(
        matrices[index].data(), n, index < 2 ? n : nrhs, Layout(index), 4,
        matrices[index].storage()));
  }
  auto Input(unsigned int index) {
    return static_cast<DenseBlasMatrixView<const T>>(Matrix(index));
  }
  template <typename E>
  auto Vector(Page<E>& page, int count) {
    return Take(
        DenseBlasVectorView<E>::Create(page.data(), count, 1, page.storage()));
  }
  auto Pivots() {
    return Take(RawLapackPivotView::Create(pivots.data(), n,
                                           LapackFactorFamily::kLuPartialPivot,
                                           pivots.storage()));
  }
  auto Query(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose) {
    if (mode.fact == 'N') {
      return QueryGesvxWorkspace(provider, transpose, Input(0), Matrix(1),
                                 Vector(pivots, n), Input(2), Matrix(3),
                                 Vector(ferr, nrhs), Vector(berr, nrhs),
                                 *statistics.data());
    }
    if (mode.fact == 'E') {
      return QueryGesvxEquilibratedWorkspace(
          provider, transpose, Matrix(0), Matrix(1), Vector(pivots, n),
          *equilibration.data(), Vector(rows, n), Vector(columns, n), Matrix(2),
          Matrix(3), Vector(ferr, nrhs), Vector(berr, nrhs),
          *statistics.data());
    }
    return QueryGesvxFactoredWorkspace(
        provider, transpose, Input(0), Input(1), Pivots(),
        *equilibration.data(),
        static_cast<DenseBlasVectorView<const Real<T>>>(Vector(rows, n)),
        static_cast<DenseBlasVectorView<const Real<T>>>(Vector(columns, n)),
        Matrix(2), Matrix(3), Vector(ferr, nrhs), Vector(berr, nrhs),
        *statistics.data());
  }
  auto Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose transpose, const LapackWorkspacePlan& plan,
               const LapackWorkspace& work, LapackReport& report) {
    if (mode.fact == 'N') {
      return Gesvx(provider, transpose, Input(0), Matrix(1), Vector(pivots, n),
                   Input(2), Matrix(3), Vector(ferr, nrhs), Vector(berr, nrhs),
                   *statistics.data(), plan, work, report);
    }
    if (mode.fact == 'E') {
      return GesvxEquilibrated(
          provider, transpose, Matrix(0), Matrix(1), Vector(pivots, n),
          *equilibration.data(), Vector(rows, n), Vector(columns, n), Matrix(2),
          Matrix(3), Vector(ferr, nrhs), Vector(berr, nrhs), *statistics.data(),
          plan, work, report);
    }
    return GesvxFactored(
        provider, transpose, Input(0), Input(1), Pivots(),
        *equilibration.data(),
        static_cast<DenseBlasVectorView<const Real<T>>>(Vector(rows, n)),
        static_cast<DenseBlasVectorView<const Real<T>>>(Vector(columns, n)),
        Matrix(2), Matrix(3), Vector(ferr, nrhs), Vector(berr, nrhs),
        *statistics.data(), plan, work, report);
  }
  void ProtectMatrices() {
    for (auto& matrix : matrices) {
      matrix.Protect();
    }
  }
  void ProtectOutputs() {
    matrices[3].Protect();
    statistics.Protect();
    ferr.Protect();
    berr.Protect();
    if (mode.fact != 'F') {
      matrices[1].Protect();
      pivots.Protect();
    }
    if (mode.fact == 'E') {
      rows.Protect();
      columns.Protect();
      equilibration.Protect();
    } else if (mode.fact == 'F') {
      if (!Rows(mode)) {
        rows.Protect();
      }
      if (!Columns(mode)) {
        columns.Protect();
      }
    }
  }
  void OpenAll() {
    for (auto& matrix : matrices) {
      matrix.Open();
    }
    pivots.Open();
    rows.Open();
    columns.Open();
    ferr.Open();
    berr.Open();
    statistics.Open();
    equilibration.Open();
  }
  void SetBoundary(DenseBlasTranspose transpose, const LapackWorkspace& work) {
    expected = {};
    expected.fact = mode.fact;
    expected.equilibration = Equed(mode);
    if (transpose == kTranspose) {
      expected.transpose = 'T';
    } else if (transpose == kConjugate) {
      expected.transpose = 'C';
    }
    expected.nrhs = nrhs;
    auto* cursor =
        static_cast<T*>(work.regions[asc_lu_aux_info_test::kLayout].data());
    for (unsigned int index = 0; index < matrices.size(); ++index) {
      expected.dimensions[index] = Layout(index) == kRow ? n : 4;
      expected.matrices[index] = matrices[index].data();
      if (Layout(index) == kRow) {
        expected.matrices[index] = cursor;
        const int count = n * (index < 2 ? n : nrhs);
        if (count != 0) {
          cursor += count;
        }
      }
    }
    expected.reals = {mode.fact == 'N' ? nullptr : rows.data(),
                      mode.fact == 'N' ? nullptr : columns.data(),
                      &statistics.data()->reciprocal_condition, ferr.data(),
                      berr.data()};
    expected.pivots = work.regions[asc_lu_aux_info_test::kInteger].data();
  }
};

template <typename T>
thread_local Frame<T>* active_frame = nullptr;
template <typename T>
void OpenOutputs() {
  active_frame<T>->OpenAll();
}

void Enter(const char* fact, const char* trans, const char* equed,
           const lapack_int* n, const lapack_int* nrhs,
           std::array<lapack_int, 4> dimensions,
           std::array<const void*, 4> matrices,
           std::array<const void*, 5> reals, const void* pivots) {
  if (*fact != expected.fact || *trans != expected.transpose ||
      *equed != expected.equilibration || *n != 3 || *nrhs != expected.nrhs ||
      dimensions != expected.dimensions || matrices != expected.matrices ||
      reals != expected.reals || pivots != expected.pivots ||
      entry_hook == nullptr) {
    std::abort();
  }
  ++entries;
  entry_hook();
}

enum class Phase : std::uint8_t { kQuery, kStale, kShort, kNative, kLocal };

template <typename T>
void ExecuteObserved(TestContext& test, Frame<T>& frame,
                     const ReferenceLapackProvider& provider,
                     DenseBlasTranspose transpose, Phase phase,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace) {
  LapackReport report;
  if (phase == Phase::kLocal) {
    frame.ProtectMatrices();
    frame.pivots.Protect();
    frame.rows.Protect();
    frame.columns.Protect();
  } else {
    frame.SetBoundary(transpose, workspace);
    active_frame<T> = &frame;
    entry_hook = &OpenOutputs<T>;
    frame.ProtectOutputs();
  }
  ASC_DENSE_TEST_CHECK(
      test, frame.Execute(provider, transpose, plan, workspace, report).ok());
  if (phase == Phase::kLocal) {
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, entries, 0U);
    ASC_DENSE_TEST_EQ(test, frame.statistics.data()->reciprocal_condition, 1);
    ASC_DENSE_TEST_EQ(test, frame.statistics.data()->reciprocal_pivot_growth,
                      1);
    for (int j = 0; j < frame.nrhs; ++j) {
      ASC_DENSE_TEST_EQ(test, frame.ferr.data()[j], Real<T>{0});
      ASC_DENSE_TEST_EQ(test, frame.berr.data()[j], Real<T>{0});
    }
    if (frame.mode.fact == 'E') {
      ASC_DENSE_TEST_EQ(test, *frame.equilibration.data(),
                        LapackEquilibration::kNone);
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
  LapackReport report;
  unsigned checks = 1;
  if (phase == Phase::kStale || phase == Phase::kShort) {
    frame.ProtectMatrices();
    frame.ProtectOutputs();
    // Supplied pivot and selected scale validation legitimately reads inputs.
    if (phase == Phase::kStale) {
      auto stale = plan;
      stale.total_byte_limit = 0;
      ASC_DENSE_TEST_EQ(
          test,
          frame.Execute(provider, transpose, stale, workspace, report).code(),
          ErrorCode::kInvalidState);
    } else {
      checks = 0;
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
        ASC_DENSE_TEST_CHECK(test,
                             !report.called_provider && !report.native_info);
        ++checks;
      }
    }
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.real, before.real);
    ASC_DENSE_TEST_EQ(test, scratch.packing, before.packing);
    ASC_DENSE_TEST_EQ(test, scratch.integers, before.integers);
    ASC_DENSE_TEST_EQ(test, entries, 0U);
    return checks;
  }
  ExecuteObserved(test, frame, provider, transpose, phase, plan, workspace);
  scratch.Guards(test);
  entry_hook = nullptr;
  active_frame<T> = nullptr;
  return checks;
}

template <typename T>
bool ObservePhase(Phase phase) {
  TestContext test;
  const auto provider =
      Take(ReferenceLapackProvider::Create(ExecutionContext::Serial()));
  unsigned cases = 0;
  unsigned checks = 0;
  for (auto mode : kModes) {
    for (auto transpose : {kNone, kTranspose, kConjugate}) {
      for (unsigned int layouts = 0; layouts < 16; ++layouts) {
        for (auto shape : {std::array{0, 0}, std::array{0, 2}, std::array{3, 0},
                           std::array{3, 2}}) {
          const bool local = shape[0] == 0;
          if ((phase == Phase::kNative && local) ||
              (phase == Phase::kLocal && !local)) {
            continue;
          }
          Frame<T> frame(shape[0], shape[1], layouts, mode);
          checks += ObserveCase(test, frame, provider, transpose, phase);
          ++cases;
        }
      }
    }
  }
  const unsigned expected_cases =
      phase == Phase::kNative || phase == Phase::kLocal ? 576 : 1152;
  ASC_DENSE_TEST_EQ(test, cases, expected_cases);
  std::fprintf(stderr, "GESVX observer bytes=%zu phase=%d cases=%u checks=%u\n",
               sizeof(T), static_cast<int>(phase), cases, checks);
  return test.Finish() == 0;
}
template <typename T, typename E>
void DetectOutputRead(TestContext& test, Frame<T>& frame, E* output) {
  ASC_DENSE_TEST_CHECK(test, Child(
                                 [&] {
                                   frame.ProtectOutputs();
                                   volatile E old_output = *output;
                                   static_cast<void>(old_output);
                                   return true;
                                 },
                                 true));
}

template <typename T>
void CalibrateInputs(TestContext& test) {
  const auto provider =
      Take(ReferenceLapackProvider::Create(ExecutionContext::Serial()));
  Frame<T> supplied(3, 2, 15, {'F', LapackEquilibration::kBoth});
  const auto plan = Take(supplied.Query(provider, kNone));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  for (unsigned input = 0; input < 4; ++input) {
    ASC_DENSE_TEST_CHECK(
        test, Child(
                  [&] {
                    if (input == 0) {
                      supplied.pivots.Protect();
                    } else if (input == 1) {
                      supplied.rows.Protect();
                    } else if (input == 2) {
                      supplied.columns.Protect();
                    } else {
                      supplied.matrices[1].Protect();
                      LapackReport report;
                      return supplied
                          .Execute(provider, kNone, plan, workspace, report)
                          .ok();
                    }
                    return supplied.Query(provider, kNone).ok();
                  },
                  true));
  }
  Frame<T> fresh(3, 2, 5, {'N', LapackEquilibration::kNone});
  const auto fresh_plan = Take(fresh.Query(provider, kNone));
  const auto fresh_work = scratch.Workspace(fresh_plan);
  for (unsigned input : {0U, 2U}) {
    ASC_DENSE_TEST_CHECK(test, Child(
                                   [&] {
                                     fresh.matrices[input].Protect();
                                     LapackReport report;
                                     return fresh
                                         .Execute(provider, kNone, fresh_plan,
                                                  fresh_work, report)
                                         .ok();
                                   },
                                   true));
  }
}

template <typename T>
void Calibrate(TestContext& test) {
  unsigned reads = 0;
  unsigned writes = 0;
  for (auto mode : kModes) {
    Frame<T> frame(3, 2, 15, mode);
    DetectOutputRead(test, frame, frame.matrices[3].data());
    for (auto* output : {frame.ferr.data(), frame.berr.data(),
                         &frame.statistics.data()->reciprocal_condition,
                         &frame.statistics.data()->reciprocal_pivot_growth}) {
      DetectOutputRead(test, frame, output);
    }
    reads += 5;
    if (mode.fact != 'F') {
      DetectOutputRead(test, frame, frame.matrices[1].data());
      DetectOutputRead(test, frame, frame.pivots.data());
      reads += 2;
    }
    if (mode.fact == 'E') {
      DetectOutputRead(test, frame, frame.rows.data());
      DetectOutputRead(test, frame, frame.columns.data());
      DetectOutputRead(test, frame, frame.equilibration.data());
      reads += 3;
    }
    ASC_DENSE_TEST_CHECK(
        test, Child([&] {
          frame.ProtectOutputs();
          frame.OpenAll();
          frame.matrices[3].data()[0] = T{1};
          frame.ferr.data()[0] = 0;
          frame.berr.data()[0] = 0;
          *frame.statistics.data() = {1, 1};
          if (mode.fact != 'F') {
            frame.matrices[1].data()[0] = T{1};
            frame.pivots.data()[0] = 1;
          }
          if (mode.fact == 'E') {
            frame.rows.data()[0] = 1;
            frame.columns.data()[0] = 1;
            *frame.equilibration.data() = LapackEquilibration::kNone;
          }
          return frame.matrices[3].data()[0] == T{1} &&
                 frame.ferr.data()[0] == 0 && frame.berr.data()[0] == 0 &&
                 frame.statistics.data()->reciprocal_condition == 1 &&
                 frame.statistics.data()->reciprocal_pivot_growth == 1;
        }));
    ++writes;
  }
  CalibrateInputs<T>(test);
  ASC_DENSE_TEST_EQ(test, reads, 37U);
  ASC_DENSE_TEST_EQ(test, writes, 6U);
  std::fprintf(stderr,
               "GESVX calibration bytes=%zu forbidden_reads=%u "
               "restored_writes=%u legitimate_reads=6\n",
               sizeof(T), reads, writes);
}

template <typename T>
void Run(TestContext& test) {
  Calibrate<T>(test);
  for (auto phase : {Phase::kQuery, Phase::kStale, Phase::kShort,
                     Phase::kNative, Phase::kLocal}) {
    ASC_DENSE_TEST_CHECK(test, Child([&] { return ObservePhase<T>(phase); }));
  }
}
}  // namespace asc::internal_gesvx_test

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  asc::internal_gesvx_test::TestContext test;
  asc::internal_gesvx_test::Run<float>(test);
  asc::internal_gesvx_test::Run<double>(test);
  asc::internal_gesvx_test::Run<std::complex<float>>(test);
  asc::internal_gesvx_test::Run<std::complex<double>>(test);
  std::printf(
      "GESVX observer:four scalar types;N/E/F with supplied EQUED N/R/C/B;"
      "N/T/C;16 independent layouts;four shape paths.216 calibrated/phase "
      "children. ASC pre-native scope; supplied inputs remain readable;"
      "local estimates/statistics writable and source-reviewed; native-return "
      "quality scans permitted. Actual phase records count cases/checks.\n");
  return test.Finish();
}

// Reserved spellings implement GNU test-only foreign-entry interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" void __real_sgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, float* a, const lapack_int* lda, float* af,
    const lapack_int* ldaf, lapack_int* pivots, char* equed, float* rows,
    float* columns, float* b, const lapack_int* ldb, float* x,
    const lapack_int* ldx, float* rcond, float* ferr, float* berr, float* work,
    lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
);
extern "C" void __wrap_sgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, float* a, const lapack_int* lda, float* af,
    const lapack_int* ldaf, lapack_int* pivots, char* equed, float* rows,
    float* columns, float* b, const lapack_int* ldb, float* x,
    const lapack_int* ldx, float* rcond, float* ferr, float* berr, float* work,
    lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (fact_length != 1 || trans_length != 1 || equed_length != 1) {
    std::abort();
  }
#endif
  asc::internal_gesvx_test::Enter(fact, trans, equed, n, nrhs,
                                  {*lda, *ldaf, *ldb, *ldx}, {a, af, b, x},
                                  {rows, columns, rcond, ferr, berr}, pivots);
  __real_sgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                 columns, b, ldb, x, ldx, rcond, ferr, berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 fact_length, trans_length, equed_length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_sgesvx_), decltype(&LAPACK_sgesvx_base)>);
extern "C" void __real_dgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, double* a, const lapack_int* lda, double* af,
    const lapack_int* ldaf, lapack_int* pivots, char* equed, double* rows,
    double* columns, double* b, const lapack_int* ldb, double* x,
    const lapack_int* ldx, double* rcond, double* ferr, double* berr,
    double* work, lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
);
extern "C" void __wrap_dgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, double* a, const lapack_int* lda, double* af,
    const lapack_int* ldaf, lapack_int* pivots, char* equed, double* rows,
    double* columns, double* b, const lapack_int* ldb, double* x,
    const lapack_int* ldx, double* rcond, double* ferr, double* berr,
    double* work, lapack_int* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (fact_length != 1 || trans_length != 1 || equed_length != 1) {
    std::abort();
  }
#endif
  asc::internal_gesvx_test::Enter(fact, trans, equed, n, nrhs,
                                  {*lda, *ldaf, *ldb, *ldx}, {a, af, b, x},
                                  {rows, columns, rcond, ferr, berr}, pivots);
  __real_dgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                 columns, b, ldb, x, ldx, rcond, ferr, berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 fact_length, trans_length, equed_length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_dgesvx_), decltype(&LAPACK_dgesvx_base)>);
extern "C" void __real_cgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, std::complex<float>* a, const lapack_int* lda,
    std::complex<float>* af, const lapack_int* ldaf, lapack_int* pivots,
    char* equed, float* rows, float* columns, std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* x, const lapack_int* ldx,
    float* rcond, float* ferr, float* berr, std::complex<float>* work,
    float* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
);
extern "C" void __wrap_cgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, std::complex<float>* a, const lapack_int* lda,
    std::complex<float>* af, const lapack_int* ldaf, lapack_int* pivots,
    char* equed, float* rows, float* columns, std::complex<float>* b,
    const lapack_int* ldb, std::complex<float>* x, const lapack_int* ldx,
    float* rcond, float* ferr, float* berr, std::complex<float>* work,
    float* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (fact_length != 1 || trans_length != 1 || equed_length != 1) {
    std::abort();
  }
#endif
  asc::internal_gesvx_test::Enter(fact, trans, equed, n, nrhs,
                                  {*lda, *ldaf, *ldb, *ldx}, {a, af, b, x},
                                  {rows, columns, rcond, ferr, berr}, pivots);
  __real_cgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                 columns, b, ldb, x, ldx, rcond, ferr, berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 fact_length, trans_length, equed_length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_cgesvx_), decltype(&LAPACK_cgesvx_base)>);
extern "C" void __real_zgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, std::complex<double>* a, const lapack_int* lda,
    std::complex<double>* af, const lapack_int* ldaf, lapack_int* pivots,
    char* equed, double* rows, double* columns, std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* rcond, double* ferr, double* berr, std::complex<double>* work,
    double* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
);
extern "C" void __wrap_zgesvx_(
    const char* fact, const char* trans, const lapack_int* n,
    const lapack_int* nrhs, std::complex<double>* a, const lapack_int* lda,
    std::complex<double>* af, const lapack_int* ldaf, lapack_int* pivots,
    char* equed, double* rows, double* columns, std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* rcond, double* ferr, double* berr, std::complex<double>* work,
    double* extra, lapack_int* info
#ifdef LAPACK_FORTRAN_STRLEN_END
    ,
    FORTRAN_STRLEN fact_length, FORTRAN_STRLEN trans_length,
    FORTRAN_STRLEN equed_length
#endif
) {
#ifdef LAPACK_FORTRAN_STRLEN_END
  if (fact_length != 1 || trans_length != 1 || equed_length != 1) {
    std::abort();
  }
#endif
  asc::internal_gesvx_test::Enter(fact, trans, equed, n, nrhs,
                                  {*lda, *ldaf, *ldb, *ldx}, {a, af, b, x},
                                  {rows, columns, rcond, ferr, berr}, pivots);
  __real_zgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, rows,
                 columns, b, ldb, x, ldx, rcond, ferr, berr, work, extra, info
#ifdef LAPACK_FORTRAN_STRLEN_END
                 ,
                 fact_length, trans_length, equed_length
#endif
  );
}
static_assert(
    std::is_same_v<decltype(&__wrap_zgesvx_), decltype(&LAPACK_zgesvx_base)>);
// NOLINTEND(bugprone-reserved-identifier)
