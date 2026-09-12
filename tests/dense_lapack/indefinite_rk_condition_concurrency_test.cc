#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_condition_test_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_rk_condition_test::Condition;
using asc_rk_condition_test::Query;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;

struct Mode {
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  bool blocked;
};

template <typename T, std::size_t Size>
class ReadOnlyArray {
 public:
  ReadOnlyArray(TestContext& test, const std::array<T, Size>& values)
      : data_(values.data()) {
#if defined(__linux__)
    const auto page = sysconf(_SC_PAGESIZE);
    ASC_DENSE_TEST_CHECK(test, page > 0);
    if (page <= 0) {
      valid_ = false;
      return;
    }
    bytes_ = ((sizeof(values) + static_cast<std::size_t>(page) - 1) /
              static_cast<std::size_t>(page)) *
             static_cast<std::size_t>(page);
    memory_ = mmap(nullptr, bytes_, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASC_DENSE_TEST_CHECK(test, memory_ != MAP_FAILED);
    if (memory_ == MAP_FAILED) {
      memory_ = nullptr;
      valid_ = false;
      return;
    }
    std::memcpy(memory_, values.data(), sizeof(values));
    const auto result = mprotect(memory_, bytes_, PROT_READ);
    ASC_DENSE_TEST_EQ(test, result, 0);
    valid_ = result == 0;
    data_ = static_cast<const T*>(memory_);
#else
    (void)test;
#endif
  }
  ReadOnlyArray(const ReadOnlyArray&) = delete;
  ReadOnlyArray& operator=(const ReadOnlyArray&) = delete;
  ReadOnlyArray(ReadOnlyArray&&) = delete;
  ReadOnlyArray& operator=(ReadOnlyArray&&) = delete;
  ~ReadOnlyArray() {
#if defined(__linux__)
    if (memory_ != nullptr) {
      munmap(memory_, bytes_);
    }
#endif
  }
  [[nodiscard]] bool valid() const { return valid_; }
  [[nodiscard]] const T* data() const { return data_; }
  void Check(TestContext& test, const std::array<T, Size>& original) const {
    ASC_DENSE_TEST_CHECK(
        test, base::EqualBytes(data_, original.data(), sizeof(original)));
  }

 private:
  const T* data_;
  bool valid_ = true;
#if defined(__linux__)
  void* memory_ = nullptr;
  std::size_t bytes_ = 0;
#endif
};
template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  Mode mode;
  bool hermitian;
  Real scale;
  std::array<T, 5000> a{};
  std::array<asc::index_t, 72> pivots{};
  std::array<T, 72> e{};
  Sample(Mode selected, bool he, bool singular)
      : mode(selected),
        hermitian(he),
        scale(std::ldexp(Real{1}, mode.n == 7 ? -20 : 20)) {
    a.fill(base::Value<T>(-31, 17));
    pivots.fill(-37);
    e.fill(base::Value<T>(-39, 19));
    for (int j = 0; j < mode.n; ++j) {
      for (int i = 0; i < mode.n; ++i) {
        if ((mode.triangle == base::kUpper && i <= j) ||
            (mode.triangle == base::kLower && i >= j)) {
          T value{};
          if (i == j && i >= 2) {
            value = base::Value<T>(i % 2 == 0 ? 8 : -8, hermitian ? 0 : 6);
          }
          if (i + j == 1) {
            value = base::Value<T>(3, hermitian && i == 0 ? -4 : 4);
          }
          if (singular && i == mode.n - 1 && i == j) {
            value = T{};
          }
          const int offset = mode.layout == base::kColumn
                                 ? j * (mode.n + 2) + i
                                 : i * (mode.n + 2) + j;
          a[1U + static_cast<std::size_t>(offset)] = scale * value;
        }
      }
    }
  }
  [[nodiscard]] Real Norm() const {
    return scale *
           (asc::DenseBlasComplex<T> && !hermitian ? Real{10} : Real{8});
  }
  [[nodiscard]] Real Expected() const {
    const Real smallest = asc::DenseBlasComplex<T> ? Real{5} : Real{3};
    return smallest * scale / Norm();
  }
  [[nodiscard]] auto View() const {
    return base::Matrix(a, mode.n, mode.n, mode.layout, mode.n + 2);
  }
  [[nodiscard]] auto Raw() const { return base::Raw(pivots, mode.n); }
  void Factor(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool singular) {
    auto matrix = base::Matrix(a, mode.n, mode.n, mode.layout, mode.n + 2);
    auto pivot = base::Pivots(pivots, mode.n);
    const auto plan = base::Take(asc_rk_test::QueryFactor(
        provider, mode.triangle, hermitian, mode.blocked, matrix,
        asc_rk_test::OffDiagonal(e, mode.n), pivot));
    base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status = asc_rk_test::Factor(
        provider, mode.triangle, hermitian, mode.blocked, matrix,
        asc_rk_test::OffDiagonal(e, mode.n), pivot, plan, workspace, report);
    scratch.Guards(test, workspace);
    ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1),
                      singular ? mode.n : 0);
    for (int i = 0; i < mode.n;) {
      const bool pair = pivots[static_cast<std::size_t>(i) + 1] < 0;
      const int ignored = pair && mode.triangle == base::kLower ? i + 1 : i;
      e[static_cast<std::size_t>(ignored) + 1] =
          base::Value<T>(std::numeric_limits<Real>::quiet_NaN(), 19);
      i += pair ? 2 : 1;
    }
  }
};

template <typename T>
struct Ready {
  using Real = asc::DenseBlasRealType<T>;
  Mode mode;
  bool hermitian;
  Real norm;
  Real expected;
  asc::DenseBlasMatrixView<const T> factors;
  asc::DenseBlasVectorView<const T> extra;
  asc::RawLapackPivotView pivots;
  [[nodiscard]] Real Norm() const { return norm; }
  [[nodiscard]] Real Expected() const { return expected; }
  [[nodiscard]] auto View() const { return factors; }
  [[nodiscard]] auto Extra() const { return extra; }
  [[nodiscard]] auto Raw() const { return pivots; }
};
template <typename T>
Ready<T> Inputs(const Sample<T>& sample, const ReadOnlyArray<T, 5000>& a,
                const ReadOnlyArray<T, 72>& e,
                const ReadOnlyArray<asc::index_t, 72>& p) {
  return {sample.mode,
          sample.hermitian,
          sample.Norm(),
          sample.Expected(),
          base::Take(asc::DenseBlasMatrixView<const T>::Create(
              a.data() + 1, sample.mode.n, sample.mode.n, sample.mode.layout,
              sample.mode.n + 2, {a.data(), sizeof(sample.a), base::kHost})),
          base::Take(asc::DenseBlasVectorView<const T>::Create(
              e.data() + 1, sample.mode.n, 1,
              {e.data(), sizeof(sample.e), base::kHost})),
          base::Take(asc::RawLapackPivotView::Create(
              p.data() + 1, sample.mode.n, asc::LapackFactorFamily::kRook,
              {p.data(), sizeof(sample.pivots), base::kHost}))};
}
template <typename Real>
void Estimate(TestContext& test, const asc::Status& status,
              const asc::LapackReport& report, Real condition, Real expected,
              bool active) {
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.called_provider, active);
  ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), active);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 0);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
  ASC_DENSE_TEST_CHECK(test, std::abs(condition - expected) <=
                                 32 * std::numeric_limits<Real>::epsilon());
}

template <typename T>
int Worker(const asc::ReferenceLapackProvider& provider, const Ready<T>& good,
           const Ready<T>& singular, const asc::LapackWorkspacePlan& active,
           const asc::LapackWorkspacePlan& zero, int worker,
           std::barrier<>& barrier) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  asc_rk_condition_test::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(active);
  asc::LapackWorkspace empty;
  auto stale = active;
  ++stale.regions[base::kScalar].preferred_entries;
  asc::LapackReport report;
  const auto& sample = worker == 1 ? singular : good;
  const auto& plan = worker == 2 ? zero : active;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    Real condition = -13;
    barrier.arrive_and_wait();
    const auto status = Condition(
        provider, sample.mode.triangle, sample.hermitian, sample.View(),
        sample.Extra(), sample.Raw(), worker == 2 ? Real{} : sample.Norm(),
        condition, worker == 3 ? stale : plan, worker == 2 ? empty : workspace,
        report);
    barrier.arrive_and_wait();
    if (worker == 3) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, condition, Real{-13});
      ASC_DENSE_TEST_EQ(test, report.factor_family,
                        asc::LapackFactorFamily::kRook);
    } else {
      Estimate(test, status, report, condition,
               worker == 0 ? good.Expected() : Real{}, worker != 2);
    }
    // Each report must recover independently for a subsequent shared-factor
    // call.
    barrier.arrive_and_wait();
    const auto ordinary = Condition(
        provider, good.mode.triangle, good.hermitian, good.View(), good.Extra(),
        good.Raw(), good.Norm(), condition, active, workspace, report);
    Estimate(test, ordinary, report, condition, good.Expected(), true);
    scratch.Guards(test, workspace);
  }
  return test.Finish();
}

template <typename T>
void Group(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Mode mode, bool hermitian) {
  using Real = asc::DenseBlasRealType<T>;
  Sample<T> good(mode, hermitian, false);
  good.Factor(test, provider, false);
  Sample<T> singular(mode, hermitian, true);
  singular.Factor(test, provider, true);
  const auto before_good = good;
  const auto before_singular = singular;
  ReadOnlyArray ga(test, good.a);
  ReadOnlyArray ge(test, good.e);
  ReadOnlyArray gp(test, good.pivots);
  ReadOnlyArray sa(test, singular.a);
  ReadOnlyArray se(test, singular.e);
  ReadOnlyArray sp(test, singular.pivots);
  if (!ga.valid() || !ge.valid() || !gp.valid() || !sa.valid() || !se.valid() ||
      !sp.valid()) {
    return;
  }
  const auto ready_good = Inputs(good, ga, ge, gp);
  const auto ready_singular = Inputs(singular, sa, se, sp);
  Real output = -13;
  const auto active = base::Take(Query(provider, mode.triangle, hermitian,
                                       ready_good.View(), ready_good.Extra(),
                                       ready_good.Raw(), good.Norm(), output));
  const auto zero =
      base::Take(Query(provider, mode.triangle, hermitian, ready_good.View(),
                       ready_good.Extra(), ready_good.Raw(), Real{}, output));
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  std::array<int, kWorkers> result{};
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[static_cast<std::size_t>(worker)] = std::thread([&, worker] {
      result[static_cast<std::size_t>(worker)] = Worker(
          provider, ready_good, ready_singular, active, zero, worker, barrier);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const int status : result) {
    ASC_DENSE_TEST_EQ(test, status, 0);
  }
  ga.Check(test, before_good.a);
  ge.Check(test, before_good.e);
  gp.Check(test, before_good.pivots);
  sa.Check(test, before_singular.a);
  se.Check(test, before_singular.e);
  sp.Check(test, before_singular.pivots);
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(good.a.data(), before_good.a.data(), sizeof(good.a)) &&
          base::EqualBytes(good.e.data(), before_good.e.data(),
                           sizeof(good.e)) &&
          good.pivots == before_good.pivots);
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(singular.a.data(), before_singular.a.data(),
                             sizeof(singular.a)) &&
                base::EqualBytes(singular.e.data(), before_singular.e.data(),
                                 sizeof(singular.e)) &&
                singular.pivots == before_singular.pivots);
}
template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int groups = 0;
  for (const int n : {7, 67}) {
    for (const auto triangle : {base::kUpper, base::kLower}) {
      for (const auto layout : {base::kColumn, base::kRow}) {
        for (const bool blocked : {false, true}) {
          Group<T>(test, provider, {n, triangle, layout, blocked}, hermitian);
          ++groups;
        }
      }
    }
  }
  std::printf(
      "RK condition concurrency groups=%d workers=%d rounds=%d native=768 "
      "zero_noncalls=128 structural_rejections=128 readonly_factor_sets=2\n",
      groups, kWorkers, kRepeats);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(false);
  }
  if (scalar == "d") {
    return Run<double>(false);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(false);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(false);
  }
  if (scalar == "ch") {
    return Run<std::complex<float>>(true);
  }
  if (scalar == "zh") {
    return Run<std::complex<double>>(true);
  }
  return 2;
}
