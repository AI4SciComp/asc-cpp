#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
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
void GuardThreshold(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle,
                    const std::array<asc::DenseBlasLayout, 4>& layouts,
                    int branch) {
  using Real = asc::DenseBlasRealType<T>;
  // N=1, KD=0 gives NZ=2, EPS=IEEE epsilon/2, SAFE1=2*minimum.
  const Real safe1 = Real{2} * std::numeric_limits<Real>::min();
  const Real safe2 = safe1 / (std::numeric_limits<Real>::epsilon() / 2);
  Real a = safe2 / 2;
  if (branch < 0) {
    a = std::nextafter(a, Real{0});
  }
  if (branch > 0) {
    a = std::nextafter(a, std::numeric_limits<Real>::infinity());
  }
  Fixture<T> data(1, 0, 1, triangle, layouts);
  data.a.values[data.a.Index(0, 0)] =
      Value<T>(a, std::numeric_limits<Real>::quiet_NaN());
  data.af.values[data.af.Index(0, 0)] = T{std::sqrt(a)};
  data.b.values[data.b.Index(0, 0)] = T{a};
  data.x.values[data.x.Index(0, 0)] = T{1};
  const auto plan = Take(data.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return data.Execute(provider, plan,
                                                   storage.View(), report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(0, 0)], T{1});
  const long double denominator = 2 * static_cast<long double>(a);
  const long double expected =
      branch > 0 ? 0
                 : static_cast<long double>(safe1) /
                       (denominator + static_cast<long double>(safe1));
  ASC_DENSE_TEST_CHECK(test,
                       std::abs(data.berr[1] - expected) <=
                           4 * std::numeric_limits<Real>::epsilon() * expected);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.ferr[1]) && data.ferr[1] >= 0);
  storage.Check(test);
}

template <typename T>
void ZeroRhs(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> data(1, 0, 1, kLower, {kRow, kColumn, kRow, kColumn});
  data.a.values[data.a.Index(0, 0)] = T{1};
  data.af.values[data.af.Index(0, 0)] = T{1};
  data.b.values[data.b.Index(0, 0)] = T{};
  data.x.values[data.x.Index(0, 0)] = T{};
  const auto plan = Take(data.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return data.Execute(provider, plan,
                                                   storage.View(), report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, data.berr[1], Real{1});
  ASC_DENSE_TEST_EQ(test, data.x.values[data.x.Index(0, 0)], T{});
  ASC_DENSE_TEST_CHECK(test, std::isfinite(data.ferr[1]) && data.ferr[1] >= 0);
  storage.Check(test);
}

template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::extent_t n, asc::extent_t nrhs) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> data(n, 0, nrhs, kLower, {kRow, kRow, kRow, kRow});
  if (n == 0) {
    const asc::stride_t wide =
        static_cast<asc::stride_t>(std::numeric_limits<std::int32_t>::max()) +
        1;
    data.a.ld = wide;
    data.af.ld = wide;
    data.b.ld = wide;
    data.x.ld = wide;
  }
  const auto nan = Value<T>(std::numeric_limits<Real>::quiet_NaN(),
                            std::numeric_limits<Real>::quiet_NaN());
  std::fill(data.a.values.begin(), data.a.values.end(), nan);
  std::fill(data.af.values.begin(), data.af.values.end(), nan);
  std::fill(data.b.values.begin(), data.b.values.end(), nan);
  std::fill(data.x.values.begin(), data.x.values.end(), nan);
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto b = data.b.values;
  const auto x = data.x.values;
  const auto plan =
      Take(WithoutAllocation(test, [&] { return data.Query(provider); }));
  for (const auto& role : plan.regions) {
    ASC_DENSE_TEST_EQ(test, role.minimum_entries, 0);
  }
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return data.Execute(provider, plan, {}, report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(
      test, SameBytes(a, data.a.values) && SameBytes(af, data.af.values));
  ASC_DENSE_TEST_CHECK(
      test, SameBytes(b, data.b.values) && SameBytes(x, data.x.values));
  for (asc::extent_t j = 0; j < nrhs; ++j) {
    ASC_DENSE_TEST_EQ(test, data.ferr[static_cast<std::size_t>(j + 1)],
                      Real{0});
    ASC_DENSE_TEST_EQ(test, data.berr[static_cast<std::size_t>(j + 1)],
                      Real{0});
  }
  ASC_DENSE_TEST_EQ(test, data.ferr.front(), Real{-317});
  ASC_DENSE_TEST_EQ(test, data.ferr.back(), Real{-317});
  ASC_DENSE_TEST_EQ(test, data.berr.front(), Real{-331});
  ASC_DENSE_TEST_EQ(test, data.berr.back(), Real{-331});
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      for (const int branch : {-1, 0, 1}) {
        GuardThreshold<T>(test, provider, triangle, layouts, branch);
      }
    }
  }
  ZeroRhs<T>(test, provider);
  Empty<T>(test, provider, 0, 0);
  Empty<T>(test, provider, 0, 2);
  Empty<T>(test, provider, 3, 0);
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
