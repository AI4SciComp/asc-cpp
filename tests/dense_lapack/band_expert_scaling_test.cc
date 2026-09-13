#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::Fixture;
using asc_band_estimation_test::InverseOneNorm;
using asc_band_estimation_test::OneNorm;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void CheckScaled(TestContext& test, const Fixture<T>& fixture,
                 const std::vector<T>& original_b) {
  using Real = asc::DenseBlasRealType<T>;
  const auto& data = fixture.data;
  auto factors = data.af;
  const auto n = data.a.n;
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto si = static_cast<long double>(
        fixture.scales[static_cast<std::size_t>(i + 1)]);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(si) && si > 0);
    for (asc::extent_t j = 0; j < n; ++j) {
      const auto sj = static_cast<long double>(
          fixture.scales[static_cast<std::size_t>(j + 1)]);
      const auto index = static_cast<std::size_t>(i * n + j);
      const auto expected = si * data.a.original[index] * sj;
      factors.original[index] = expected;
      if (data.a.Selected(i, j)) {
        const auto actual = ToWide(data.a.values[data.a.Index(i, j)]);
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(actual - expected) <=
                                 4 * std::numeric_limits<Real>::epsilon() *
                                     std::max(1.0L, std::abs(expected)));
      }
    }
    for (asc::extent_t j = 0; j < data.b.count; ++j) {
      const auto index = data.b.Index(i, j);
      const T expected =
          fixture.scales[static_cast<std::size_t>(i + 1)] * original_b[index];
      ASC_DENSE_TEST_EQ(test, data.b.values[index], expected);
    }
  }
  factors.CheckFactor(test);
  const auto count = static_cast<std::size_t>(n);
  const auto expected_rcond = 1 / (OneNorm(factors.original, count) *
                                   InverseOneNorm(factors.original, count));
  // This is an estimate on this fixed well-conditioned fixture, not an exact
  // inverse norm promise. The separate analytic KD=1 gate is stricter.
  ASC_DENSE_TEST_CHECK(test,
                       fixture.reciprocal_condition >= expected_rcond / 2 &&
                           fixture.reciprocal_condition <= 2 * expected_rcond);
}

template <typename T>
void CheckForward(TestContext& test, const Fixture<T>& fixture) {
  const auto& x = fixture.data.x;
  for (asc::extent_t j = 0; j < x.count; ++j) {
    long double difference = 0;
    long double norm = 0;
    for (asc::extent_t i = 0; i < x.n; ++i) {
      const auto actual = ToWide(x.values[x.Index(i, j)]);
      const auto expected =
          x.expected[static_cast<std::size_t>(i * x.count + j)];
      difference =
          std::max(difference, std::abs((actual - expected).real()) +
                                   std::abs((actual - expected).imag()));
      norm = std::max(norm, std::abs(actual.real()) + std::abs(actual.imag()));
    }
    const auto ferr = fixture.data.ferr[static_cast<std::size_t>(j + 1)];
    const auto berr = fixture.data.berr[static_cast<std::size_t>(j + 1)];
    ASC_DENSE_TEST_CHECK(test, std::isfinite(ferr) && ferr >= 0);
    ASC_DENSE_TEST_CHECK(test, std::isfinite(berr) && berr >= 0);
    ASC_DENSE_TEST_CHECK(test, difference / norm <= ferr);
  }
}

template <typename T>
void Reuse(TestContext& test, const asc::ReferenceLapackProvider& provider,
           Fixture<T>& fixture, const std::vector<T>& original_b) {
  auto& data = fixture.data;
  fixture.mode = 'F';
  data.b.values = original_b;
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto scales = fixture.scales;
  const auto prior_x = data.x.values;
  for (asc::extent_t i = 0; i < data.x.n; ++i) {
    for (asc::extent_t j = 0; j < data.x.count; ++j) {
      data.x.values[data.x.Index(i, j)] = T{-419};
    }
  }
  const auto plan =
      Take(WithoutAllocation(test, [&] { return fixture.Query(provider); }));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return fixture.Execute(provider, plan,
                                                      storage.View(), report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(scales, fixture.scales));
  ASC_DENSE_TEST_CHECK(test, SameBytes(prior_x, data.x.values));
  data.x.Check(test, data.a, prior_x);
  CheckScaled(test, fixture, original_b);
  CheckForward(test, fixture);
  storage.Check(test);
}

template <typename T>
void Scaled(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasTriangle triangle,
            const std::array<asc::DenseBlasLayout, 4>& layouts, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> fixture(6, 2, 2, triangle, layouts, 'E');
  auto& data = fixture.data;
  data.a = BandData<T>(6, 2, triangle, layouts[0], exponent);
  data.b = RhsData<T>(data.a, 2, layouts[2]);
  data.x = RhsData<T>(data.a, 2, layouts[3]);
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto b = data.b.values;
  const auto x = data.x.values;
  if constexpr (asc::DenseBlasComplex<T>) {
    for (asc::extent_t i = 0; i < data.a.n; ++i) {
      data.a.values[data.a.Index(i, i)].imag(
          std::numeric_limits<Real>::quiet_NaN());
    }
  }
  const auto plan =
      Take(WithoutAllocation(test, [&] { return fixture.Query(provider); }));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return fixture.Execute(provider, plan,
                                                      storage.View(), report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, fixture.equilibration,
                    asc::LapackCholeskyEquilibration::kDiagonal);
  data.x.Check(test, data.a, x);
  data.a.CheckPadding(test, a);
  data.af.CheckPadding(test, af);
  CheckScaled(test, fixture, b);
  CheckForward(test, fixture);
  storage.Check(test);
  Reuse(test, provider, fixture, b);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int scale = sizeof(Real) == 4 ? 60 : 510;
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      Scaled<T>(test, provider, triangle, layouts, scale);
      Scaled<T>(test, provider, triangle, layouts, -scale);
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
