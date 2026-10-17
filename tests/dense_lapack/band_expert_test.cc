#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::Fixture;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void Initialize(TestContext& test, const asc::ReferenceLapackProvider& provider,
                Fixture<T>& fixture) {
  using Real = asc::DenseBlasRealType<T>;
  auto& data = fixture.data;
  if (fixture.mode == 'F') {
    const auto plan = Take(asc::QueryPbtrfWorkspace(provider, data.af.View()));
    Storage<T> storage(plan);
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(
        test, asc::Pbtrf(provider, data.af.View(), plan, storage.View(), report)
                  .ok());
    fixture.scale_count = 0;
  } else {
    for (asc::extent_t i = 0; i < data.af.n; ++i) {
      for (asc::extent_t j = 0; j < data.af.n; ++j) {
        if (data.af.Selected(i, j)) {
          data.af.values[data.af.Index(i, j)] =
              Value<T>(std::numeric_limits<Real>::quiet_NaN(),
                       std::numeric_limits<Real>::quiet_NaN());
        }
      }
    }
  }
  if constexpr (asc::DenseBlasComplex<T>) {
    for (asc::extent_t i = 0; i < data.a.n; ++i) {
      data.a.values[data.a.Index(i, i)].imag(
          std::numeric_limits<Real>::quiet_NaN());
    }
  }
  for (asc::extent_t i = 0; i < data.x.n; ++i) {
    for (asc::extent_t j = 0; j < data.x.count; ++j) {
      data.x.values[data.x.Index(i, j)] =
          Value<T>(std::numeric_limits<Real>::quiet_NaN(),
                   std::numeric_limits<Real>::quiet_NaN());
    }
  }
}

template <typename T>
void General(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::extent_t n, asc::extent_t kd, asc::extent_t nrhs,
             asc::DenseBlasTriangle triangle,
             const std::array<asc::DenseBlasLayout, 4>& layouts, char mode) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> fixture(n, kd, nrhs, triangle, layouts, mode);
  Initialize(test, provider, fixture);
  auto& data = fixture.data;
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto b = data.b.values;
  const auto x = data.x.values;
  const auto scales = fixture.scales;
  const auto plan =
      Take(WithoutAllocation(test, [&] { return fixture.Query(provider); }));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return fixture.Execute(provider, plan, storage.View(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, fixture.equilibration,
                    asc::LapackCholeskyEquilibration::kNone);
  data.x.Check(test, data.a, x);
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b, data.b.values));
  if (mode == 'F') {
    ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.af.values));
  } else {
    data.af.CheckFactor(test);
  }
  if (mode != 'E') {
    ASC_DENSE_TEST_CHECK(test, SameBytes(scales, fixture.scales));
  }
  ASC_DENSE_TEST_CHECK(test, std::isfinite(fixture.reciprocal_condition) &&
                                 fixture.reciprocal_condition > 0);
  if (n == 0) {
    ASC_DENSE_TEST_EQ(test, fixture.reciprocal_condition, Real{1});
  }
  for (asc::extent_t j = 0; j < nrhs; ++j) {
    const auto at = static_cast<std::size_t>(j + 1);
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(data.ferr[at]) && data.ferr[at] >= 0);
    ASC_DENSE_TEST_CHECK(test,
                         std::isfinite(data.berr[at]) && data.berr[at] >= 0);
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
        for (const auto shape : std::array<std::array<asc::extent_t, 2>, 5>{
                 {{0, 0}, {1, 0}, {4, 0}, {6, 2}, {4, 9}}}) {
          for (const asc::extent_t count : {0, 2}) {
            General<T>(test, provider, shape[0], shape[1], count, triangle,
                       layouts, mode);
          }
        }
      }
    }
    for (const char mode : {'N', 'E', 'F'}) {
      General<T>(test, provider, 96, 65, 2, triangle,
                 {kRow, kColumn, kRow, kColumn}, mode);
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
