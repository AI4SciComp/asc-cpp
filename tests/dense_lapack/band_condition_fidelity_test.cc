#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_condition.h"
#include "band_cholesky_test_support.h"
#include "band_condition_native_test_support.h"
#include "band_estimation_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_condition_test::Direct;
using asc_band_condition_test::SetAnalyticFactor;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void Compare(TestContext& test, const asc::ReferenceLapackProvider& provider,
             BandData<T>& band, asc::DenseBlasRealType<T> norm) {
  using Real = asc::DenseBlasRealType<T>;
  const auto before = band.values;
  lapack_int direct_info = -1;
  const Real direct = Direct(test, band, norm, direct_info);
  std::array<Real, 3> output{Real{-277}, Real{-281}, Real{-283}};
  const auto plan = Take(
      asc::QueryPbconWorkspace(provider, band.ConstView(), norm, output[1]));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbcon(provider, band.ConstView(), norm, output[1], plan,
                      storage.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, direct_info, 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), direct_info);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(output[1], direct));
  const bool quality = !std::isfinite(direct) || direct < 0;
  ASC_DENSE_TEST_EQ(test, status.ok(), !quality);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    quality ? asc::LapackOutcome::kAccuracyWarning
                            : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, output.front(), Real{-277});
  ASC_DENSE_TEST_EQ(test, output.back(), Real{-283});
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kTiny = std::is_same_v<Real, float> ? -60 : -510;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const int exponent : {0, kTiny}) {
        BandData<T> band(2, 1, triangle, layout);
        SetAnalyticFactor(band, exponent);
        const Real t = std::ldexp(Real{1}, exponent);
        Compare(test, provider, band, Real{3} * t * t);
      }
      for (const T raw : {Value<T>(2, 0.5), Value<T>(-2, 0.5), T{0},
                          T{std::numeric_limits<Real>::denorm_min()},
                          T{std::numeric_limits<Real>::max()},
                          T{std::numeric_limits<Real>::infinity()},
                          T{std::numeric_limits<Real>::quiet_NaN()}}) {
        BandData<T> band(1, 4, triangle, layout);
        band.values[band.Index(0, 0)] = raw;
        Compare(test, provider, band, Real{1});
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
