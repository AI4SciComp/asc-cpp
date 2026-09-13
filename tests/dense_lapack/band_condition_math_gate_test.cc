#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

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
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  constexpr int kTiny = std::is_same_v<Real, float> ? -60 : -510;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const int exponent : {0, kTiny}) {
        BandData<T> band(2, 1, triangle, layout);
        asc_band_condition_test::SetAnalyticFactor(band, exponent);
        const Real t = std::ldexp(Real{1}, exponent);
        const Real norm = Real{3} * t * t;
        Real result = -1;
        const auto plan = Take(
            asc::QueryPbconWorkspace(provider, band.ConstView(), norm, result));
        asc_band_estimation_test::WorkspaceStorage<T> storage(plan);
        asc::LapackReport report;
        ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                     return asc::Pbcon(
                                         provider, band.ConstView(), norm,
                                         result, plan, storage.View(), report);
                                   }).ok());
        // This exact 2x2 estimator fixture has reciprocal condition 1/9.
        // A known lower-complex scaled LATBS defect makes this required
        // mathematical gate FAIL on the pinned source. Do not mark WILL_FAIL,
        // skip it, substitute upper storage, or weaken the independent bound.
        const long double exact = 1.0L / 9.0L;
        const bool accurate =
            std::abs(static_cast<long double>(result) - exact) <=
            32 * std::numeric_limits<Real>::epsilon();
        std::printf(
            "triangle=%c layout=%c factor_exp=%d rcond=%.17g expected=%.17Lg "
            "INFO=%lld\n",
            triangle == kUpper ? 'U' : 'L', layout == kColumn ? 'C' : 'R',
            exponent, static_cast<double>(result), exact,
            static_cast<long long>(report.native_info.value_or(-1)));
        ASC_DENSE_TEST_CHECK(test, accurate);
        storage.Check(test);
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
