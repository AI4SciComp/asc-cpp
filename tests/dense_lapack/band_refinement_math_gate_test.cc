#include <array>
#include <cmath>
#include <complex>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_refinement_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_estimation_test::WorkspaceStorage;
using asc_band_refinement_test::Fixture;

template <typename T>
void FiniteEstimate(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle,
                    const std::array<asc::DenseBlasLayout, 4>& layouts,
                    int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  const Real factor = std::ldexp(Real{1}, exponent);
  const Real a = factor * factor;
  Fixture<T> data(1, 0, 1, triangle, layouts);
  data.a.values[data.a.Index(0, 0)] = T{a};
  data.af.values[data.af.Index(0, 0)] = T{factor};
  data.b.values[data.b.Index(0, 0)] = T{a};
  data.x.values[data.x.Index(0, 0)] = T{1};
  // Independent scalar proof: XTRUE=1 and residual=0. The guarded weighted
  // inverse norm (NZ*EPS*2*A + SAFE1)/A is finite even when 1/A overflows.
  const long double eps = std::numeric_limits<Real>::epsilon() / 2;
  const long double safe1 =
      2 * static_cast<long double>(std::numeric_limits<Real>::min());
  const long double denominator = 2 * static_cast<long double>(a);
  const long double weight =
      2 * eps * denominator + (denominator <= safe1 / eps ? safe1 : 0);
  const long double bound = weight / static_cast<long double>(a);
  ASC_DENSE_TEST_CHECK(test, a > 0 && std::isfinite(bound) &&
                                 bound < std::numeric_limits<Real>::max());
  const auto plan = Take(data.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, plan, storage.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(0, 0)], T{1});
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.berr[1]));
  // This required mathematical gate deliberately remains failing for the
  // pinned provider's tiny input. Raw fidelity is tested separately.
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.ferr[1]) && data.ferr[1] >= 0);
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      FiniteEstimate<T>(test, provider, triangle, layouts,
                        sizeof(Real) == 4 ? -50 : -450);
      FiniteEstimate<T>(test, provider, triangle, layouts,
                        sizeof(Real) == 4 ? -70 : -520);
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
