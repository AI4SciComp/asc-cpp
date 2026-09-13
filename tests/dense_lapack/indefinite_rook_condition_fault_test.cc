#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_rook_condition_faults.h"
#include "indefinite_rook_condition_test_support.h"
#include "indefinite_rook_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "lapack_build_config.h"

namespace {
namespace base = asc_indefinite_rook_test;
namespace faults = asc_rook_condition_fault_test;
using base::TestContext;
using faults::Fault;

template <typename Real>
void Outcome(TestContext& test, Fault fault, const asc::Status& status,
             const asc::LapackReport& report, Real condition) {
  const bool pass = fault == Fault::kPass || (fault == Fault::kWrite32BitZero &&
                                              ASC_LAPACK_INTEGER_BITS == 32);
  const bool nonfinite =
      fault == Fault::kNanCondition || fault == Fault::kInfCondition;
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, std::abs(faults::LastNativeCondition() - 1) <=
                                 32 * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, status.code(),
                    pass ? asc::ErrorCode::kOk
                         : (nonfinite ? asc::ErrorCode::kNumerical
                                      : asc::ErrorCode::kProvider));
  ASC_DENSE_TEST_EQ(
      test, report.output_validity,
      pass ? asc::LapackOutputValidity::kComplete
           : (nonfinite ? asc::LapackOutputValidity::kDocumentedPartial
                        : asc::LapackOutputValidity::kUnusable));
  if (fault == Fault::kOmitInfo || (fault == Fault::kWrite32BitZero && !pass)) {
    const auto sentinel =
        (ASC_LAPACK_INTEGER_BITS == 32
             ? std::int64_t{std::numeric_limits<std::int32_t>::min()}
             : std::numeric_limits<std::int64_t>::min());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), sentinel);
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  } else {
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1),
                      fault == Fault::kNegativeInfo
                          ? -6
                          : (fault == Fault::kPositiveInfo ? 7 : 0));
  }
  if (fault == Fault::kOmitCondition) {
    ASC_DENSE_TEST_EQ(test, condition, Real{-1});
  } else if (fault == Fault::kNegativeCondition) {
    ASC_DENSE_TEST_EQ(test, condition, Real{-2});
  } else if (nonfinite) {
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_CHECK(test, fault == Fault::kNanCondition
                                   ? std::isnan(condition)
                                   : std::isinf(condition));
  } else {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(condition));
    ASC_DENSE_TEST_CHECK(test, std::abs(condition - Real{1}) <=
                                   32 * std::numeric_limits<Real>::epsilon());
  }
}

template <typename T>
void Check(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool hermitian, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, int n, Fault fault) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 12> a;
  a.fill(base::Value<T>(-91, 7));
  auto matrix = base::Matrix(a, n, n, layout, 3);
  if (n == 1) {
    a[1] = T{4};
  } else {
    a[1] = T{};
    a[5] = T{};
    const T off = base::Value<T>(3, 4);
    a[2] = off;
    a[4] = hermitian ? base::Value<T>(3, -4) : off;
  }
  const Real pair_norm = asc::DenseBlasComplex<T> ? Real{5} : Real{3};
  const Real norm = n == 1 ? Real{4} : pair_norm;
  std::array<asc::index_t, 4> pivots{};
  auto mutable_pivots = base::Pivots(pivots, n);
  const auto fp = base::Take(base::QueryFactor(provider, triangle, hermitian,
                                               false, matrix, mutable_pivots));
  base::Scratch<T> scratch;
  auto workspace = scratch.Workspace(fp);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test, base::Factor(provider, triangle, hermitian, false, matrix,
                         mutable_pivots, fp, workspace, report)
                .ok());
  const auto saved_a = a;
  const auto saved_pivots = pivots;
  const auto view = base::Matrix(saved_a, n, n, layout, 3);
  const auto raw = base::Raw(pivots, n);
  std::array<Real, 3> condition{Real{-101}, Real{-13}, Real{-103}};
  const auto plan = base::Take(asc_rook_condition_test::Query(
      provider, triangle, hermitian, view, raw, norm, condition[1]));
  scratch = base::Scratch<T>{};
  workspace = scratch.Workspace(plan);
  faults::SetFault(fault);
  const auto status = base::WithoutAllocation(test, [&] {
    return asc_rook_condition_test::Condition(provider, triangle, hermitian,
                                              view, raw, norm, condition[1],
                                              plan, workspace, report);
  });
  scratch.Guards(test, workspace);
  Outcome(test, fault, status, report, condition[1]);
  ASC_DENSE_TEST_EQ(test, condition.front(), Real{-101});
  ASC_DENSE_TEST_EQ(test, condition.back(), Real{-103});
  ASC_DENSE_TEST_CHECK(test,
                       base::EqualBytes(a.data(), saved_a.data(), sizeof(a)));
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(pivots.data(), saved_pivots.data(), sizeof(pivots)));
}

template <typename T>
int Run(bool hermitian) {
  TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (const auto triangle : {base::kUpper, base::kLower}) {
    for (const auto layout : {base::kColumn, base::kRow}) {
      for (const int n : {1, 2}) {
        for (const auto fault :
             {Fault::kPass, Fault::kOmitInfo, Fault::kWrite32BitZero,
              Fault::kNegativeInfo, Fault::kPositiveInfo, Fault::kOmitCondition,
              Fault::kNegativeCondition, Fault::kNanCondition,
              Fault::kInfCondition}) {
          Check<T>(test, provider, hermitian, triangle, layout, n, fault);
          ++cases;
        }
      }
    }
  }
  std::printf("fault cases=%d integer_bits=%d\n", cases,
              ASC_LAPACK_INTEGER_BITS);
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
