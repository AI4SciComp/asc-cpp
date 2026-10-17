#include <cmath>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_svd_faults.h"
#include "scalar_svd_test_support.h"
namespace {
namespace svd = asc_scalar_svd_test;
namespace fault = asc_scalar_svd_fault;
namespace numerical = asc_scalar_svd_numerical;
using fault::Mode;
template <typename T>
void Case(svd::TestContext& test, const asc::ReferenceLapackProvider& provider,
          Mode mode) {
  svd::Output<T> output;
  const auto before = output.values;
  const auto views = output.Views();
  asc::LapackReport report;
  fault::Reset(mode);
  const auto status = svd::WithoutAllocation(test, [&] {
    return svd::Execute(provider, T{3}, T{4}, T{5}, views, report);
  });
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 1U);
  ASC_DENSE_TEST_EQ(test, fault::UnexpectedNativeResults(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  const bool warning = mode == Mode::kPositiveInfiniteLarge ||
                       mode == Mode::kNegativeInfiniteLarge;
  const bool complete =
      warning || mode == Mode::kPass || mode == Mode::kWrongFinite ||
      mode == Mode::kZeroTuple || mode == Mode::kAtCoefficientLimit;
  if (!complete) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, output.values, before);
  } else {
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        warning ? asc::ErrorCode::kNumerical : asc::ErrorCode::kOk);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      warning ? asc::LapackOutcome::kAccuracyWarning
                              : asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    const auto& v = output.values;
    if (warning) {
      ASC_DENSE_TEST_CHECK(test, std::isinf(v[1][1]));
      ASC_DENSE_TEST_EQ(test, std::signbit(v[1][1]),
                        mode == Mode::kNegativeInfiniteLarge);
    } else if (mode == Mode::kAtCoefficientLimit) {
      ASC_DENSE_TEST_EQ(test, v[5][1],
                        T{1} + T{16} * std::numeric_limits<T>::epsilon());
    } else {
      ASC_DENSE_TEST_EQ(
          test,
          (numerical::Accurate(T{3}, T{4}, T{5}, v[0][1], v[1][1], v[2][1],
                               v[3][1], v[4][1], v[5][1])),
          mode == Mode::kPass);
    }
  }
  for (const auto& v : output.values) {
    ASC_DENSE_TEST_EQ(test, v[0], T{71});
    ASC_DENSE_TEST_EQ(test, v[2], T{73});
  }
}
template <typename T>
void Run(svd::TestContext& test) {
  const auto provider = svd::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (const auto mode : {Mode::kPass,
                          Mode::kOmitSmall,
                          Mode::kOmitLarge,
                          Mode::kOmitRightSine,
                          Mode::kOmitRightCosine,
                          Mode::kOmitLeftSine,
                          Mode::kOmitLeftCosine,
                          Mode::kNanSmall,
                          Mode::kInfiniteSmall,
                          Mode::kNanLarge,
                          Mode::kPositiveInfiniteLarge,
                          Mode::kNegativeInfiniteLarge,
                          Mode::kNanRightSine,
                          Mode::kLargeRightSine,
                          Mode::kNanRightCosine,
                          Mode::kLargeRightCosine,
                          Mode::kNanLeftSine,
                          Mode::kLargeLeftSine,
                          Mode::kNanLeftCosine,
                          Mode::kLargeLeftCosine,
                          Mode::kInfiniteCoefficient,
                          Mode::kAtCoefficientLimit,
                          Mode::kBeyondCoefficientLimit,
                          Mode::kChangeF,
                          Mode::kChangeG,
                          Mode::kChangeH,
                          Mode::kUnsorted,
                          Mode::kWrongFinite,
                          Mode::kZeroTuple}) {
    Case<T>(test, provider, mode);
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  svd::TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else {
    return 2;
  }
  return test.Finish();
}
