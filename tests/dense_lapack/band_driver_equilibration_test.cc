#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "asc/dense/providers/lapack_cholesky_band_driver.h"
#include "asc/dense/providers/lapack_cholesky_band_equilibration.h"
#include "band_cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)

template <typename Real>
asc::DenseBlasVectorView<Real> Vector(std::vector<Real>& data,
                                      asc::extent_t count) {
  return Take(asc::DenseBlasVectorView<Real>::Create(
      data.data() + 1, count, 1,
      {data.data(), data.size() * sizeof(Real), kHost}));
}
void Success(TestContext& test, const asc::ReferenceLapackProvider& provider,
             const asc::LapackReport& report, std::string_view stem) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()).substr(1),
                    stem);
}

template <typename T>
void Driver(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::extent_t n, asc::extent_t kd, asc::DenseBlasTriangle triangle,
            asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
            asc::extent_t nrhs, int scale = 0) {
  BandData<T> band(n, kd, triangle, layout, scale);
  RhsData<T> rhs(band, nrhs, rhs_layout);
  const auto before = band.values;
  const auto rhs_before = rhs.values;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPbsvWorkspace(provider, band.View(), rhs.View());
  }));
  const auto expected =
      (layout == kRow ? n * (kd + 1) : 0) + (rhs_layout == kRow ? n * nrhs : 0);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries, expected);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].preferred_entries, expected);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, rhs.values));
  Storage<T> storage(plan);
  auto workspace = storage.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbsv(provider, band.View(), rhs.View(), plan, workspace,
                     report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  Success(test, provider, report, "pbsv");
  band.CheckFactor(test);
  band.CheckPadding(test, before);
  rhs.Check(test, band, rhs_before);
  storage.Check(test);
  // The actual driver's successful band factor is reusable without refactoring.
  RhsData<T> next(band, 3, rhs_layout);
  const auto next_before = next.values;
  const auto factor = band.values;
  const auto next_plan =
      Take(asc::QueryPbtrsWorkspace(provider, band.ConstView(), next.View()));
  Storage<T> next_storage(next_plan);
  auto next_workspace = next_storage.View();
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Pbtrs(provider, band.ConstView(),
                                                 next.View(), next_plan,
                                                 next_workspace, report);
                             }).ok());
  next.Check(test, band, next_before);
  ASC_DENSE_TEST_CHECK(test, SameBytes(factor, band.values));
  next_storage.Check(test);
}

template <typename T>
void DriverFailure(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                   asc::DenseBlasLayout rhs_layout, asc::extent_t failed) {
  BandData<T> band(6, 2, triangle, layout);
  for (asc::extent_t j = 0; j < band.n; ++j) {
    for (asc::extent_t i = 0; i < band.n; ++i) {
      if (band.Selected(i, j)) {
        band.values[band.Index(i, j)] = Value<T>(i == j ? 4 : 0);
      }
    }
  }
  band.values[band.Index(failed, failed)] = Value<T>(-1, 99);
  RhsData<T> rhs(band, 2, rhs_layout);
  const auto before = band.values;
  const auto rhs_before = rhs.values;
  const auto plan =
      Take(asc::QueryPbsvWorkspace(provider, band.View(), rhs.View()));
  Storage<T> storage(plan);
  auto workspace = storage.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbsv(provider, band.View(), rhs.View(), plan, workspace,
                     report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), failed + 1);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), failed);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    asc::LapackOutcome::kNotPositiveDefinite);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, rhs.values));
  for (asc::extent_t i = 0; i < failed; ++i) {
    ASC_DENSE_TEST_CHECK(
        test, SameScalarBytes(band.values[band.Index(i, i)], Value<T>(2)));
  }
  band.CheckPadding(test, before);
  storage.Check(test);
}

template <typename Real>
void CheckEquilibrationValues(
    TestContext& test, const std::vector<Real>& diagonal,
    const std::vector<Real>& scales,
    const asc::LapackBandEquilibrationStatistics<Real>& statistics,
    int exponent, asc::extent_t failed) {
  const auto n = static_cast<asc::extent_t>(diagonal.size());
  Real minimum = n == 0 ? Real{1} : diagonal.front();
  Real maximum = 0;
  for (asc::extent_t i = 0; i < n; ++i) {
    const auto a = diagonal[static_cast<std::size_t>(i)];
    minimum = std::min(minimum, a);
    maximum = std::max(maximum, a);
    // Dyadic positive diagonals have exactly representable reciprocal roots.
    const auto expected =
        failed >= 0
            ? a
            : std::ldexp(Real{1}, -(static_cast<int>(i % 3) + exponent));
    ASC_DENSE_TEST_CHECK(
        test,
        SameScalarBytes(scales[static_cast<std::size_t>(i + 1)], expected));
  }
  ASC_DENSE_TEST_CHECK(test,
                       SameScalarBytes(statistics.absolute_maximum, maximum));
  if (failed < 0) {
    const auto expected =
        n == 0 ? Real{1} : std::sqrt(minimum) / std::sqrt(maximum);
    ASC_DENSE_TEST_CHECK(test,
                         SameScalarBytes(statistics.scale_condition, expected));
  }
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(scales.front(), Real{-37}));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(scales.back(), Real{-37}));
}

template <typename T>
void Equilibration(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc::extent_t n, asc::extent_t kd,
                   asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                   int exponent = 0, asc::extent_t failed = -1) {
  using Real = asc::DenseBlasRealType<T>;
  const auto nan = std::numeric_limits<Real>::quiet_NaN();
  BandData<T> band(n, kd, triangle, layout);
  std::fill(band.values.begin(), band.values.end(), Value<T>(nan, nan));
  std::vector<Real> diagonal(static_cast<std::size_t>(n));
  for (asc::extent_t i = 0; i < n; ++i) {
    diagonal[static_cast<std::size_t>(i)] =
        std::ldexp(Real{1}, 2 * (static_cast<int>(i % 3) + exponent));
    if (i == failed) {
      diagonal[static_cast<std::size_t>(i)] = -1;
    }
    band.values[band.Index(i, i)] =
        Value<T>(diagonal[static_cast<std::size_t>(i)], nan);
  }
  std::vector<Real> scales(static_cast<std::size_t>(n + 2), Real{-37});
  const auto scales_before = scales;
  const auto before = band.values;
  asc::LapackBandEquilibrationStatistics<Real> statistics{Real{-17}, Real{-29}};
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryPbequWorkspace(provider, band.ConstView(),
                                    Vector(scales, n), statistics);
  }));
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(scales_before, scales));
  ASC_DENSE_TEST_CHECK(test,
                       SameScalarBytes(statistics.scale_condition, Real{-17}));
  ASC_DENSE_TEST_CHECK(test,
                       SameScalarBytes(statistics.absolute_maximum, Real{-29}));
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                    layout == kRow ? n * (kd + 1) : 0);
  Storage<T> storage(plan);
  const auto packing_before = storage.values;
  auto workspace = storage.View();
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbequ(provider, band.ConstView(), Vector(scales, n), statistics,
                      plan, workspace, report);
  });
  if (failed >= 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), failed + 1);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), failed);
    ASC_DENSE_TEST_CHECK(
        test, SameScalarBytes(statistics.scale_condition, Real{-17}));
  } else {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    Success(test, provider, report, "pbequ");
  }
  CheckEquilibrationValues(test, diagonal, scales, statistics, exponent,
                           failed);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  if (layout == kRow) {
    const auto diagonal_row = triangle == kUpper ? kd : 0;
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t row = 0; row <= kd; ++row) {
        if (row != diagonal_row) {
          const auto index = static_cast<std::size_t>(1 + j * (kd + 1) + row);
          ASC_DENSE_TEST_CHECK(test, SameScalarBytes(storage.values[index],
                                                     packing_before[index]));
        }
      }
    }
  }
  storage.Check(test);
}

template <typename T>
void EmptyWideStride(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const asc::stride_t wide =
      static_cast<asc::stride_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  const asc::ConstMemoryView empty{nullptr, 0, kHost};
  for (const auto triangle : {kUpper, kLower}) {
    const auto band = Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        nullptr, 0, 0, triangle, kRow, wide, empty));
    const auto original =
        Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
            nullptr, 0, 0, triangle, kRow, wide, empty));
    const auto rhs = Take(
        asc::DenseBlasMatrixView<T>::Create(nullptr, 0, 2, kRow, wide, empty));
    const auto scales =
        Take(asc::DenseBlasVectorView<Real>::Create(nullptr, 0, 1, empty));
    asc::LapackReport report;
    asc::LapackBandEquilibrationStatistics<Real> statistics{Real{-3}, Real{-5}};
    const auto driver_plan = Take(WithoutAllocation(
        test, [&] { return asc::QueryPbsvWorkspace(provider, band, rhs); }));
    ASC_DENSE_TEST_EQ(test, driver_plan.regions[kLayout].minimum_entries, 0);
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Pbsv(provider, band, rhs,
                                                  driver_plan, {}, report);
                               }).ok());
    Success(test, provider, report, "pbsv");
    const auto eq_plan = Take(WithoutAllocation(test, [&] {
      return asc::QueryPbequWorkspace(provider, original, scales, statistics);
    }));
    ASC_DENSE_TEST_EQ(test, eq_plan.regions[kLayout].minimum_entries, 0);
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Pbequ(provider, original, scales,
                                                   statistics, eq_plan, {},
                                                   report);
                               }).ok());
    Success(test, provider, report, "pbequ");
    ASC_DENSE_TEST_EQ(test, statistics.scale_condition, Real{1});
    ASC_DENSE_TEST_EQ(test, statistics.absolute_maximum, Real{0});
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  EmptyWideStride<T>(test, provider);
  constexpr int kScale =
      std::is_same_v<asc::DenseBlasRealType<T>, float> ? 50 : 450;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const auto shape : std::array<std::array<asc::extent_t, 2>, 6>{
               {{0, 0}, {1, 0}, {5, 0}, {6, 2}, {4, 9}, {96, 65}}}) {
        Equilibration<T>(test, provider, shape[0], shape[1], triangle, layout);
        for (const auto rhs_layout : {kColumn, kRow}) {
          for (const asc::extent_t nrhs : {0, 2}) {
            Driver<T>(test, provider, shape[0], shape[1], triangle, layout,
                      rhs_layout, nrhs);
          }
        }
      }
      Equilibration<T>(test, provider, 6, 2, triangle, layout, kScale);
      Equilibration<T>(test, provider, 6, 2, triangle, layout, -kScale);
      for (const asc::extent_t failed : {0, 3, 5}) {
        Equilibration<T>(test, provider, 6, 2, triangle, layout, 0, failed);
        for (const auto rhs_layout : {kColumn, kRow}) {
          DriverFailure<T>(test, provider, triangle, layout, rhs_layout,
                           failed);
        }
      }
      for (const auto rhs_layout : {kColumn, kRow}) {
        Driver<T>(test, provider, 6, 2, triangle, layout, rhs_layout, 2,
                  kScale);
        Driver<T>(test, provider, 6, 2, triangle, layout, rhs_layout, 2,
                  -kScale);
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
