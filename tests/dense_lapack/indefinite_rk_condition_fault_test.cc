#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rk_condition_faults.h"
#include "indefinite_rk_condition_test_support.h"
#include "indefinite_rk_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
namespace {
namespace base = asc_indefinite_rook_test;
namespace faults = asc_rk_condition_fault_test;
using asc_rk_condition_test::Scratch;
using base::TestContext;
using faults::Fault;
template <typename Real>
void Outcome(TestContext& test, Fault fault, const asc::Status& status,
             const asc::LapackReport& report, Real condition) {
  const bool accepted =
      fault == Fault::kPass || fault == Fault::kZeroCondition ||
      fault == Fault::kAboveOneCondition ||
      (fault == Fault::kWrite32BitInfo && sizeof(lapack_int) == 4) ||
      (fault == Fault::kWrite32BitCondition && sizeof(Real) == 4);
  const bool nonfinite =
      fault == Fault::kNanCondition || fault == Fault::kInfCondition;
  const auto info = faults::LastPublishedInfo();
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(test, faults::SeedWasFullWidth());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, std::abs(faults::LastNativeCondition() - 1) <=
                                 32 * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-999), info);
  ASC_DENSE_TEST_EQ(test, status.code(),
                    accepted    ? asc::ErrorCode::kOk
                    : nonfinite ? asc::ErrorCode::kNumerical
                                : asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    accepted    ? asc::LapackOutputValidity::kComplete
                    : nonfinite ? asc::LapackOutputValidity::kDocumentedPartial
                                : asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    accepted    ? asc::LapackOutcome::kSuccess
                    : nonfinite ? asc::LapackOutcome::kAccuracyWarning
                    : info < 0  ? asc::LapackOutcome::kProviderArgument
                                : asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  const bool argument =
      info < 0 && info != std::numeric_limits<lapack_int>::min();
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
  if (argument) {
    ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(-999), -info);
  }
  if (fault == Fault::kNegativeCondition) {
    ASC_DENSE_TEST_EQ(test, condition, Real{-2});
  } else if (fault == Fault::kOmitCondition ||
             fault == Fault::kWrite16BitCondition ||
             (fault == Fault::kWrite32BitCondition && sizeof(Real) == 8)) {
    ASC_DENSE_TEST_CHECK(test, condition < 0);
  } else if (nonfinite) {
    ASC_DENSE_TEST_CHECK(test, fault == Fault::kNanCondition
                                   ? std::isnan(condition)
                                   : std::isinf(condition));
  } else if (fault == Fault::kZeroCondition) {
    ASC_DENSE_TEST_EQ(test, condition, Real{});
  } else if (fault == Fault::kAboveOneCondition) {
    ASC_DENSE_TEST_EQ(test, condition, Real{2});
  } else {
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(condition) &&
                             std::abs(condition - Real{1}) <=
                                 32 * std::numeric_limits<Real>::epsilon());
  }
}
template <typename T>
void ExecuteFault(TestContext& test,
                  const asc::ReferenceLapackProvider& provider, bool hermitian,
                  asc::DenseBlasTriangle triangle,
                  asc::DenseBlasMatrixView<const T> view,
                  asc::DenseBlasVectorView<const T> immutable_e,
                  asc::RawLapackPivotView raw, asc::DenseBlasRealType<T> norm,
                  Fault fault) {
  using Real = asc::DenseBlasRealType<T>;
  const auto n = view.rows();
  std::array<Real, 3> condition{Real{-101}, Real{-13}, Real{-103}};
  faults::SetFault(fault);
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return asc_rk_condition_test::Query(provider, triangle, hermitian, view,
                                        immutable_e, raw, norm, condition[1]);
  }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto before = scratch;
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return asc_rk_condition_test::Condition(
        provider, triangle, hermitian, view, immutable_e, raw, norm,
        condition[1], plan, workspace, report);
  });
  if (norm == 0 || n == 0) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, condition[1], n == 0 ? Real{1} : Real{});
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_EQ(test, scratch.scalar, before.scalar);
    ASC_DENSE_TEST_EQ(test, scratch.packed, before.packed);
    ASC_DENSE_TEST_EQ(test, scratch.pivot, before.pivot);
  } else {
    Outcome(test, fault, status, report, condition[1]);
  }
  ASC_DENSE_TEST_EQ(test, report.factor_family, asc::LapackFactorFamily::kRook);
  ASC_DENSE_TEST_EQ(test, condition.front(), Real{-101});
  ASC_DENSE_TEST_EQ(test, condition.back(), Real{-103});
  scratch.Guards(test, workspace);
}
template <typename T>
void Cases(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, int n, bool blocked, int& cases,
           int& empty) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 5000> a;
  a.fill(base::Value<T>(-91, 7));
  std::array<T, 72> e;
  e.fill(base::Value<T>(-93, 11));
  std::array<asc::index_t, 72> pivots;
  pivots.fill(-97);
  const Real pair_norm = asc::DenseBlasComplex<T> ? Real{5} : Real{3};
  const Real norm = n == 1 ? Real{4} : pair_norm;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      if (triangle == base::kUpper ? i <= j : i >= j) {
        T value{};
        if (i == j && (n == 1 || i >= 2)) {
          value = T{i % 2 == 0 ? norm : -norm};
        }
        if (i + j == 1) {
          value = base::Value<T>(3, hermitian && i == 0 ? -4 : 4);
        }
        a[1U + static_cast<std::size_t>(layout == base::kColumn
                                            ? j * (n + 2) + i
                                            : i * (n + 2) + j)] = value;
      }
    }
  }
  const auto matrix = base::Matrix(a, n, n, layout, n + 2);
  const auto extra = asc_rk_test::OffDiagonal(e, n);
  const auto mutable_pivots = base::Pivots(pivots, n);
  const auto fp = base::Take(asc_rk_test::QueryFactor(
      provider, triangle, hermitian, blocked, matrix, extra, mutable_pivots));
  Scratch<T> factor_scratch;
  const auto fw = factor_scratch.Workspace(fp);
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(
      test, asc_rk_test::Factor(provider, triangle, hermitian, blocked, matrix,
                                extra, mutable_pivots, fp, fw, factor_report)
                .ok());
  factor_scratch.Guards(test, fw);
  for (int i = 0; i < n;) {
    const bool pair = pivots[static_cast<std::size_t>(i) + 1] < 0;
    const int ignored = pair && triangle == base::kLower ? i + 1 : i;
    e[static_cast<std::size_t>(ignored) + 1] =
        base::Value<T>(std::numeric_limits<Real>::quiet_NaN(), 19);
    i += pair ? 2 : 1;
  }
  const auto saved_a = a;
  const auto saved_e = e;
  const auto saved_p = pivots;
  const auto view = base::Matrix(std::as_const(a), n, n, layout, n + 2);
  const auto raw = base::Raw(pivots, n);
  const asc::DenseBlasVectorView<const T> immutable_e(extra);
  for (const bool zero : {false, true}) {
    for (const auto fault :
         {Fault::kPass, Fault::kOmitInfo, Fault::kWrite16BitInfo,
          Fault::kWrite32BitInfo, Fault::kNegativeInfo, Fault::kPositiveInfo,
          Fault::kOutOfRangeInfo, Fault::kChangedPivot, Fault::kOmitCondition,
          Fault::kWrite16BitCondition, Fault::kWrite32BitCondition,
          Fault::kNegativeCondition, Fault::kNanCondition, Fault::kInfCondition,
          Fault::kZeroCondition, Fault::kAboveOneCondition}) {
      ExecuteFault(test, provider, hermitian, triangle, view, immutable_e, raw,
                   zero ? Real{} : norm, fault);
      if (zero || n == 0) {
        ++empty;
      }
      ASC_DENSE_TEST_CHECK(
          test, base::EqualBytes(a.data(), saved_a.data(), sizeof(a)) &&
                    base::EqualBytes(e.data(), saved_e.data(), sizeof(e)) &&
                    pivots == saved_p);
      ++cases;
    }
  }
}
template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  int empty = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const bool blocked : {false, true}) {
        for (const int n : {0, 1, 2, 7, 67}) {
          Cases<T>(test, provider, hermitian, triangle, layout, n, blocked,
                   cases, empty);
        }
      }
    }
  }
  std::printf("RK condition fault cases=%d empty_noncalls=%d\n", cases, empty);
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
