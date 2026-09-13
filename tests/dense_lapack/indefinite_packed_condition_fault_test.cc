#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "indefinite_packed_condition_faults.h"
#include "indefinite_packed_condition_test_support.h"
#include "indefinite_test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "tests/dense/test_support.h"
namespace {
namespace base = asc_indefinite_test;
namespace condition = asc_packed_condition_test;
namespace faults = asc_packed_condition_fault_test;
using faults::Fault;

bool InfoDefect(Fault fault) {
  switch (fault) {
    case Fault::kOmitInfo:
    case Fault::kWrite16BitInfo:
    case Fault::kNegativeInfo:
    case Fault::kPositiveInfo:
    case Fault::kMaximumInfo:
    case Fault::kChangedPivot:
      return true;
    case Fault::kWrite32BitInfo:
      return sizeof(lapack_int) != 4;
    default:
      return false;
  }
}

template <typename Real>
void Report(base::TestContext& test, Fault fault, Real value,
            const asc::Status& status, const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1U);
  ASC_DENSE_TEST_CHECK(
      test, faults::SeedWasFullWidth() && faults::ConditionSeedWasNan());
  ASC_DENSE_TEST_EQ(test, faults::LastNativeInfo(), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  const auto info = faults::LastPublishedInfo();
  ASC_DENSE_TEST_EQ(test, report.native_info, info);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                 !report.diagnostic_index.has_value());
  if (InfoDefect(fault)) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, value, Real{-157});
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      info < 0 ? asc::LapackOutcome::kProviderArgument
                               : asc::LapackOutcome::kPartialResult);
    const bool argument =
        info < 0 && info != std::numeric_limits<lapack_int>::min();
    ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(), argument);
    if (argument) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, -info);
    }
    return;
  }
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  const auto published = static_cast<Real>(faults::LastPublishedCondition());
  ASC_DENSE_TEST_CHECK(test,
                       (std::isnan(value) && std::isnan(published)) ||
                           base::EqualBytes(&value, &published, sizeof(Real)));
  if (value < 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
  } else if (!std::isfinite(value)) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
  }
}

template <typename T>
void Case(base::TestContext& test, const asc::ReferenceLapackProvider& provider,
          condition::Fixture<T>& sample, Fault fault) {
  using Real = asc::DenseBlasRealType<T>;
  sample.Prepare(test, provider);
  faults::SetFault(fault);
  const auto factors = sample.a.ConstView();
  const auto pivots = base::Raw(sample.a.pivots, sample.a.n);
  const auto before_a = sample.a.a;
  const auto before_pivots = sample.a.pivots;
  std::array<Real, 3> result{Real{-151}, Real{-157}, Real{-163}};
  const auto plan = base::Take(base::WithoutAllocation(test, [&] {
    return condition::Query(provider, sample.a.triangle, sample.a.hermitian,
                            factors, pivots, sample.norm, result[1]);
  }));
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
  condition::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = base::WithoutAllocation(test, [&] {
    return condition::Condition(provider, sample.a.triangle, sample.a.hermitian,
                                factors, pivots, sample.norm, result[1], plan,
                                workspace, report);
  });
  const bool active = sample.a.n != 0 && sample.norm != 0;
  if (active) {
    Report(test, fault, result[1], status, report);
    sample.Mathematics(test, static_cast<Real>(faults::LastNativeCondition()));
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0U);
    sample.Mathematics(test, result[1]);
  }
  ASC_DENSE_TEST_EQ(test, result[0], Real{-151});
  ASC_DENSE_TEST_EQ(test, result[2], Real{-163});
  ASC_DENSE_TEST_CHECK(
      test,
      base::EqualBytes(sample.a.a.data(), before_a.data(), sizeof(before_a)));
  ASC_DENSE_TEST_EQ(test, sample.a.pivots, before_pivots);
  sample.a.Guards(test);
  scratch.Guards(test, workspace);
}

template <typename T>
int Run(bool hermitian) {
  base::TestContext test;
  const auto provider = base::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  for (int n : {0, 1, 2, 5}) {
    for (auto triangle : {base::kUpper, base::kLower}) {
      for (auto layout : {base::kColumn, base::kRow}) {
        for (int kind : {0, 1, 2, 3}) {
          for (auto fault :
               {Fault::kPass, Fault::kOmitInfo, Fault::kWrite16BitInfo,
                Fault::kWrite32BitInfo, Fault::kNegativeInfo,
                Fault::kPositiveInfo, Fault::kMaximumInfo, Fault::kChangedPivot,
                Fault::kOmitCondition, Fault::kNegativeCondition,
                Fault::kNanCondition, Fault::kInfCondition,
                Fault::kNegativeInfCondition, Fault::kZeroCondition,
                Fault::kTwoCondition, Fault::kSubnormalCondition,
                Fault::kNegativeZeroCondition}) {
            condition::Fixture<T> sample(n, hermitian, triangle, layout, 0,
                                         kind);
            Case(test, provider, sample, fault);
            ++cases;
          }
        }
      }
    }
  }
  ASC_DENSE_TEST_EQ(test, cases, 1088);
  std::printf("Packed condition native fault cases=%d\n", cases);
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard normal_return;
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
