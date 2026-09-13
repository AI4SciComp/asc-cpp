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
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::Fixture;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void Tiny(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasTriangle triangle,
          const std::array<asc::DenseBlasLayout, 4>& layouts, char mode) {
  using Real = asc::DenseBlasRealType<T>;
  const Real t = std::ldexp(Real{1}, sizeof(Real) == 4 ? -70 : -520);
  const Real a = t * t;
  Fixture<T> fixture(1, 0, 1, triangle, layouts, mode);
  auto& data = fixture.data;
  data.a.values[data.a.Index(0, 0)] = T{a};
  data.af.values[data.af.Index(0, 0)] = T{t};
  data.b.values[data.b.Index(0, 0)] = T{a};
  data.x.values[data.x.Index(0, 0)] = T{-1};
  fixture.scale_count = mode == 'F' ? 0 : 1;
  const auto plan = Take(fixture.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return fixture.Execute(provider, plan, storage.View(), report);
  });
  // Independent scalar problem: positive A, condition one, exact X=1.
  // Equilibration should give (1/t)*t^2*(1/t)=1. All values are
  // representable; intermediate source reassociation is not performed here.
  ASC_DENSE_TEST_CHECK(test, a > 0 && std::isfinite(a));
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(fixture.reciprocal_condition) &&
                                 std::abs(fixture.reciprocal_condition - 1) <=
                                     16 * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.ferr[1]) && data.ferr[1] >= 0);
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(0, 0)], T{1});
  if (mode == 'E') {
    ASC_DENSE_TEST_EQ(test, data.a.values[data.a.Index(0, 0)], T{1});
  }
  storage.Check(test);
}

template <typename T>
void LowerScaledCondition(TestContext& test,
                          const asc::ReferenceLapackProvider& provider,
                          asc::DenseBlasTriangle triangle,
                          const std::array<asc::DenseBlasLayout, 4>& layouts,
                          char mode) {
  using Real = asc::DenseBlasRealType<T>;
  const Real t = std::ldexp(Real{1}, sizeof(Real) == 4 ? -60 : -510);
  const Real a = t * t;
  Fixture<T> fixture(2, 1, 1, triangle, layouts, mode);
  auto& data = fixture.data;
  data.a.values[data.a.Index(0, 0)] = T{a};
  data.a.values[data.a.Index(1, 1)] = T{2 * a};
  const int i = triangle == kLower ? 1 : 0;
  const int j = triangle == kLower ? 0 : 1;
  data.a.values[data.a.Index(i, j)] = T{a};
  data.af.values[data.af.Index(0, 0)] = T{t};
  data.af.values[data.af.Index(1, 1)] = T{t};
  data.af.values[data.af.Index(i, j)] = T{t};
  data.b.values[data.b.Index(0, 0)] = T{2 * a};
  data.b.values[data.b.Index(1, 0)] = T{3 * a};
  fixture.scale_count = 0;
  const auto plan = Take(fixture.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return fixture.Execute(provider, plan,
                                                      storage.View(), report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  // A=t²[[1,1],[1,2]], inverse=t^-2[[2,-1],[-1,1]], so rcond=1/9.
  // Keep upper and real controls in the same required test; do not replace
  // the failed complex lower route with an alternate triangle.
  ASC_DENSE_TEST_CHECK(test,
                       std::abs(fixture.reciprocal_condition - 1.0L / 9) <=
                           64 * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(0, 0)], T{1});
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(1, 0)], T{1});
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
        Tiny<T>(test, provider, triangle, layouts, mode);
      }
      for (const char mode : {'N', 'F'}) {
        LowerScaledCondition<T>(test, provider, triangle, layouts, mode);
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
