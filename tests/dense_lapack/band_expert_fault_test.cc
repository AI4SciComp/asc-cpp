#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_driver_fault_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::FaultArgumentsValid;
using asc_band_driver_test::FaultCalls;
using asc_band_driver_test::FaultConfig;
using asc_band_driver_test::Fixture;
using asc_band_driver_test::ResetFault;
using asc_band_estimation_test::WorkspaceStorage;

bool Legal(const FaultConfig& config, char mode, char supplied) {
  if (config.info < 0 || config.info > 4 ||
      (config.equilibration != 'N' && config.equilibration != 'Y')) {
    return false;
  }
  if (mode == 'N' && config.equilibration != 'N') {
    return false;
  }
  return mode != 'F' || (config.equilibration == supplied &&
                         (config.info == 0 || config.info == 4));
}
void CheckReport(TestContext& test, const asc::Status& status,
                 const asc::LapackReport& report, const FaultConfig& config,
                 bool legal) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(97), config.info);
  if (!legal) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      config.info < 0 ? asc::LapackOutcome::kProviderArgument
                                      : asc::LapackOutcome::kPartialResult);
    if (config.info < 0 &&
        config.info != std::numeric_limits<std::int64_t>::min()) {
      ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(0), -config.info);
    }
    return;
  }
  if (config.info > 0 && config.info < 4) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kNotPositiveDefinite);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1),
                      config.info - 1);
    return;
  }
  bool nonfinite = false;
  bool negative = false;
  for (const double value : {config.reciprocal_condition, config.forward_error,
                             config.backward_error}) {
    nonfinite = nonfinite || !std::isfinite(value);
    negative = negative || value < 0;
  }
  const bool warning = config.info == 4 || nonfinite || negative;
  ASC_DENSE_TEST_EQ(test, status.ok(), !warning);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    warning ? asc::LapackOutcome::kAccuracyWarning
                            : asc::LapackOutcome::kSuccess);
  if (warning) {
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        negative ? asc::ErrorCode::kProvider : asc::ErrorCode::kNumerical);
  }
}
template <typename T>
void Fault(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle,
           const std::array<asc::DenseBlasLayout, 4>& layouts, char mode,
           char supplied, FaultConfig config) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> fixture(3, 1, 2, triangle, layouts, mode);
  if (supplied == 'Y') {
    fixture.equilibration = asc::LapackCholeskyEquilibration::kDiagonal;
    std::fill(fixture.scales.begin(), fixture.scales.end(), Real{1});
  }
  auto& data = fixture.data;
  const auto before_a = data.a.values;
  const auto before_af = data.af.values;
  const auto before_x = data.x.values;
  const auto plan = Take(fixture.Query(provider));
  WorkspaceStorage<T> storage(plan);
  config.packed_factor_output = layouts[1] == kRow;
  config.packed_solution_output = layouts[3] == kRow;
  ResetFault(config);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return fixture.Execute(provider, plan, storage.View(), report);
  });
  const bool legal = Legal(config, mode, supplied);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
  CheckReport(test, status, report, config, legal);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  const bool publish_x = legal && (config.info == 0 || config.info == 4);
  for (asc::extent_t j = 0; j < data.x.count; ++j) {
    for (asc::extent_t i = 0; i < data.x.n; ++i) {
      const auto at = data.x.Index(i, j);
      ASC_DENSE_TEST_EQ(
          test, data.x.values[at],
          publish_x || layouts[3] == kColumn ? T{-457} : before_x[at]);
    }
  }
  for (asc::extent_t j = 0; j < data.a.n; ++j) {
    for (asc::extent_t i = 0; i < data.a.n; ++i) {
      if (data.a.Selected(i, j)) {
        const auto at = data.af.Index(i, j);
        const bool publish = mode != 'F' && (legal || layouts[1] == kColumn);
        ASC_DENSE_TEST_EQ(test, data.af.values[at],
                          publish ? Value<T>(-443, 73) : before_af[at]);
      }
    }
  }
  if (mode != 'E' || config.equilibration != 'Y' ||
      (!legal && (layouts[0] == kRow || asc::DenseBlasComplex<T>))) {
    ASC_DENSE_TEST_CHECK(test, SameBytes(data.a.values, before_a));
  }
  ASC_DENSE_TEST_CHECK(
      test, SameScalarBytes(fixture.reciprocal_condition,
                            static_cast<Real>(config.reciprocal_condition)));
  for (std::size_t i = 1; i <= 2; ++i) {
    ASC_DENSE_TEST_CHECK(
        test,
        SameScalarBytes(data.ferr[i], static_cast<Real>(config.forward_error)));
    ASC_DENSE_TEST_CHECK(
        test, SameScalarBytes(data.berr[i],
                              static_cast<Real>(config.backward_error)));
  }
  storage.Check(test);
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      for (const char mode : {'N', 'E', 'F'}) {
        for (const char supplied : {'N', 'Y'}) {
          if (mode != 'F' && supplied == 'Y') {
            continue;
          }
          for (const std::int64_t info :
               {std::int64_t{0}, std::int64_t{1}, std::int64_t{3},
                std::int64_t{4}, std::int64_t{5}, std::int64_t{-7},
                static_cast<std::int64_t>(
                    std::numeric_limits<lapack_int>::min())}) {
            for (const char equed : {'N', 'Y', '?'}) {
              FaultConfig config;
              config.info = info;
              config.equilibration = equed;
              Fault<T>(test, provider, triangle, layouts, mode, supplied,
                       config);
            }
          }
          for (const double raw :
               {-1.0, std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN()}) {
            for (const auto member :
                 {&FaultConfig::reciprocal_condition,
                  &FaultConfig::forward_error, &FaultConfig::backward_error}) {
              FaultConfig config;
              config.equilibration = supplied;
              config.*member = raw;
              Fault<T>(test, provider, triangle, layouts, mode, supplied,
                       config);
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
