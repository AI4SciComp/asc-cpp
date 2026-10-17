#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "asc/dense/providers/lapack_cholesky_band_condition.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_estimation_test::InverseOneNorm;
using asc_band_estimation_test::OneNorm;
using asc_band_estimation_test::WorkspaceStorage;

void Success(TestContext& test, const asc::ReferenceLapackProvider& provider,
             const asc::LapackReport& report) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()).substr(1),
                    "pbcon");
}

template <typename T>
void General(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::extent_t n, asc::extent_t kd, asc::DenseBlasTriangle triangle,
             asc::DenseBlasLayout layout, int exponent = 0) {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> band(n, kd, triangle, layout, exponent);
  const auto norm = OneNorm(band.original, static_cast<std::size_t>(n));
  const auto expected =
      n == 0 ? 1
             : 1 / (norm *
                    InverseOneNorm(band.original, static_cast<std::size_t>(n)));
  const auto factor_plan =
      Take(asc::QueryPbtrfWorkspace(provider, band.View()));
  Storage<T> factor_storage(factor_plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc::Pbtrf(provider, band.View(), factor_plan,
                                        factor_storage.View(), report)
                                 .ok());
  band.CheckFactor(test);
  const auto before = band.values;
  std::array<Real, 3> rcond{Real{-227}, Real{-229}, Real{-233}};
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPbconWorkspace(provider, band.ConstView(),
                                    static_cast<Real>(norm), rcond[1]);
  }));
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  ASC_DENSE_TEST_EQ(test, rcond[1], Real{-229});
  WorkspaceStorage<T> storage(plan);
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbcon(provider, band.ConstView(), static_cast<Real>(norm),
                      rcond[1], plan, storage.View(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  Success(test, provider, report);
  ASC_DENSE_TEST_CHECK(test, std::isfinite(rcond[1]));
  if (n <= 1 || kd == 0) {
    ASC_DENSE_TEST_CHECK(test, std::abs(rcond[1] - expected) <=
                                   16 * std::numeric_limits<Real>::epsilon());
  } else {
    // PBCON returns an estimate, not an exact inverse norm. This known
    // well-conditioned band fixture agrees with the independent wide inverse.
    ASC_DENSE_TEST_CHECK(test,
                         rcond[1] >= expected / 2 && rcond[1] <= 2 * expected);
  }
  ASC_DENSE_TEST_EQ(test, rcond.front(), Real{-227});
  ASC_DENSE_TEST_EQ(test, rcond.back(), Real{-233});
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  storage.Check(test);
  factor_storage.Check(test);
}

template <typename T>
void NoReadBranches(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle,
                    asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> band(3, 7, triangle, layout);
  const auto nan = std::numeric_limits<Real>::quiet_NaN();
  std::fill(band.values.begin(), band.values.end(), Value<T>(nan, nan));
  const auto before = band.values;
  Real rcond = -3;
  const auto plan = Take(
      asc::QueryPbconWorkspace(provider, band.ConstView(), Real{0}, rcond));
  for (const auto& role : plan.regions) {
    ASC_DENSE_TEST_EQ(test, role.minimum_entries, 0);
  }
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Pbcon(provider, band.ConstView(),
                                                 Real{0}, rcond, plan, {},
                                                 report);
                             }).ok());
  Success(test, provider, report);
  ASC_DENSE_TEST_EQ(test, rcond, Real{0});
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  const asc::stride_t wide =
      static_cast<asc::stride_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const auto empty = Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
      nullptr, 0, 0, triangle, kRow, wide, {nullptr, 0, kHost}));
  const auto empty_plan =
      Take(asc::QueryPbconWorkspace(provider, empty, Real{1}, rcond));
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Pbcon(provider, empty, Real{1},
                                                 rcond, empty_plan, {}, report);
                             }).ok());
  Success(test, provider, report);
  ASC_DENSE_TEST_EQ(test, rcond, Real{1});
}

template <typename T>
void RawSingle(TestContext& test, const asc::ReferenceLapackProvider& provider,
               asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> band(1, 4, triangle, layout);
  const auto raw = Value<T>(-2, 0.5L);
  band.values[band.Index(0, 0)] = raw;
  const auto before = band.values;
  const Real norm = static_cast<Real>(std::norm(ToWide(raw)));
  Real rcond = -7;
  const auto plan =
      Take(asc::QueryPbconWorkspace(provider, band.ConstView(), norm, rcond));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Pbcon(provider, band.ConstView(),
                                                 norm, rcond, plan,
                                                 storage.View(), report);
                             }).ok());
  Success(test, provider, report);
  ASC_DENSE_TEST_CHECK(
      test, std::abs(rcond - 1) <= 16 * std::numeric_limits<Real>::epsilon());
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  constexpr int kScale =
      std::is_same_v<asc::DenseBlasRealType<T>, float> ? 50 : 450;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const auto shape : std::array<std::array<asc::extent_t, 2>, 6>{
               {{0, 0}, {1, 0}, {5, 0}, {6, 2}, {4, 9}, {96, 65}}}) {
        General<T>(test, provider, shape[0], shape[1], triangle, layout);
      }
      General<T>(test, provider, 6, 2, triangle, layout, kScale);
      General<T>(test, provider, 6, 2, triangle, layout, -kScale);
      NoReadBranches<T>(test, provider, triangle, layout);
      RawSingle<T>(test, provider, triangle, layout);
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
