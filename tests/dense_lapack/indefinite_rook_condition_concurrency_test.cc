#include <array>
#include <barrier>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <thread>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rook_condition_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace base = asc_indefinite_rook_test;
using asc_rook_condition_test::Condition;
using asc_rook_condition_test::Query;
using base::TestContext;
constexpr int kWorkers = 4;
constexpr int kRepeats = 8;

struct Mode {
  int n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  bool blocked;
};

// Real CON needs both n converted pivots and n simultaneous IWORK entries.
// Its ILP64 order-67 fixture exceeds the factor-only scratch integer capacity.
template <typename T>
struct ConditionScratch {
  base::Scratch<T> scalar;
  alignas(16) std::array<std::byte, 1120> integers{};
  ConditionScratch() { integers.fill(std::byte{0x5a}); }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    auto scalar_plan = plan;
    scalar_plan.regions[base::kPivot] = {};
    auto workspace = scalar.Workspace(scalar_plan);
    const auto& requirement = plan.regions[base::kPivot];
    const auto bytes = static_cast<std::size_t>(requirement.minimum_entries) *
                       requirement.entry_bytes;
    if (bytes > integers.size() - 32) {
      std::abort();
    }
    workspace.regions[base::kPivot] = {integers.data() + 16, bytes,
                                       base::kHost};
    return workspace;
  }
  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    auto scalar_workspace = workspace;
    scalar_workspace.regions[base::kPivot] = {nullptr, 0, base::kHost};
    scalar.Guards(test, scalar_workspace);
    for (std::size_t i = 0; i < 16; ++i) {
      ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
    }
    for (std::size_t i = 16 + workspace.regions[base::kPivot].size();
         i < integers.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, integers[i], std::byte{0x5a});
    }
  }
};

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  Mode mode;
  bool hermitian;
  Real scale;
  std::array<T, 5000> a{};
  std::array<asc::index_t, 72> pivots{};
  Sample(Mode selected, bool he, bool singular)
      : mode(selected),
        hermitian(he),
        scale(std::ldexp(Real{1}, mode.n == 7 ? -20 : 20)) {
    a.fill(base::Value<T>(-31, 17));
    pivots.fill(-37);
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
    const auto plan = base::Take(base::QueryFactor(
        provider, mode.triangle, hermitian, mode.blocked, matrix, pivot));
    base::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status =
        base::Factor(provider, mode.triangle, hermitian, mode.blocked, matrix,
                     pivot, plan, workspace, report);
    ASC_DENSE_TEST_EQ(test, status.ok(), !singular);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1),
                      singular ? mode.n : 0);
  }
};

template <typename Real>
void Estimate(TestContext& test, const asc::Status& status,
              const asc::LapackReport& report, Real condition, Real expected,
              bool active) {
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
int Worker(const asc::ReferenceLapackProvider& provider, const Sample<T>& good,
           const Sample<T>& singular, const asc::LapackWorkspacePlan& active,
           const asc::LapackWorkspacePlan& zero, int worker,
           std::barrier<>& barrier) {
  using Real = asc::DenseBlasRealType<T>;
  TestContext test;
  ConditionScratch<T> scratch;
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
        sample.Raw(), worker == 2 ? Real{} : sample.Norm(), condition,
        worker == 3 ? stale : plan, worker == 2 ? empty : workspace, report);
    barrier.arrive_and_wait();
    if (worker == 3) {
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
      ASC_DENSE_TEST_CHECK(
          test, !report.called_provider && !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, condition, Real{-13});
    } else {
      Estimate(test, status, report, condition,
               worker == 0 ? good.Expected() : Real{}, worker != 2);
    }
    // Each report must recover independently for a subsequent shared-factor
    // call.
    const auto ordinary = Condition(
        provider, good.mode.triangle, good.hermitian, good.View(), good.Raw(),
        good.Norm(), condition, active, workspace, report);
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
  const auto before_good = good.a;
  const auto before_singular = singular.a;
  const auto before_gp = good.pivots;
  const auto before_sp = singular.pivots;
  Real output = -13;
  const auto active =
      base::Take(Query(provider, mode.triangle, hermitian, good.View(),
                       good.Raw(), good.Norm(), output));
  const auto zero = base::Take(Query(provider, mode.triangle, hermitian,
                                     good.View(), good.Raw(), Real{}, output));
  std::barrier barrier(kWorkers);
  std::array<std::thread, kWorkers> threads;
  std::array<int, kWorkers> result{};
  for (int worker = 0; worker < kWorkers; ++worker) {
    threads[static_cast<std::size_t>(worker)] = std::thread([&, worker] {
      result[static_cast<std::size_t>(worker)] =
          Worker(provider, good, singular, active, zero, worker, barrier);
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (const int status : result) {
    ASC_DENSE_TEST_EQ(test, status, 0);
  }
  ASC_DENSE_TEST_CHECK(test, base::EqualBytes(good.a.data(), before_good.data(),
                                              sizeof(before_good)));
  ASC_DENSE_TEST_CHECK(
      test, base::EqualBytes(singular.a.data(), before_singular.data(),
                             sizeof(before_singular)));
  ASC_DENSE_TEST_CHECK(
      test, good.pivots == before_gp && singular.pivots == before_sp);
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
      "concurrency groups=%d workers=%d rounds=%d native=768 noncalls=256\n",
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
