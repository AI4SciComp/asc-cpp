#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_driver.h"
#include "asc/dense/providers/lapack_cholesky_band_equilibration.h"
#include "band_cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)

template <typename T>
struct ExpertNative;
template <>
struct ExpertNative<float> {
  static constexpr auto kDriver = LAPACK_spbsv_base;
  static constexpr auto kEquilibrate = LAPACK_spbequ_base;
};
template <>
struct ExpertNative<double> {
  static constexpr auto kDriver = LAPACK_dpbsv_base;
  static constexpr auto kEquilibrate = LAPACK_dpbequ_base;
};
template <>
struct ExpertNative<std::complex<float>> {
  static constexpr auto kDriver = LAPACK_cpbsv_base;
  static constexpr auto kEquilibrate = LAPACK_cpbequ_base;
};
template <>
struct ExpertNative<std::complex<double>> {
  static constexpr auto kDriver = LAPACK_zpbsv_base;
  static constexpr auto kEquilibrate = LAPACK_zpbequ_base;
};

template <typename T>
std::vector<T> ColumnCopy(const BandData<T>& band) {
  std::vector<T> result(static_cast<std::size_t>(band.n * band.ld),
                        Value<T>(-79, 31));
  for (asc::extent_t i = 0; i < band.n; ++i) {
    for (asc::extent_t j = 0; j < band.n; ++j) {
      if (band.Selected(i, j)) {
        const auto offset = band.triangle == kUpper ? band.kd + i - j : i - j;
        result[static_cast<std::size_t>(j * band.ld + offset)] =
            band.values[band.Index(i, j)];
      }
    }
  }
  return result;
}

template <typename T>
void MakeDiagonalInput(BandData<T>& band, asc::extent_t failed, bool nan) {
  const auto n = band.n;
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (band.Selected(i, j)) {
        band.values[band.Index(i, j)] =
            Value<T>(i == j ? 4 : 0, i == j ? 53 : 0);
      }
    }
  }
  if (failed >= 0) {
    band.values[band.Index(failed, failed)] =
        Value<T>(nan ? std::numeric_limits<long double>::quiet_NaN() : -1, 53);
  }
}

template <typename T>
void CheckDriverOutputs(TestContext& test, const BandData<T>& band,
                        const RhsData<T>& rhs, const std::vector<T>& direct,
                        const std::vector<T>& direct_rhs,
                        asc::extent_t failed) {
  const auto n = band.n;
  const auto kd = band.kd;
  const auto triangle = band.triangle;
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (band.Selected(i, j)) {
        const auto offset = triangle == kUpper ? kd + i - j : i - j;
        ASC_DENSE_TEST_CHECK(
            test, SameScalarBytes(
                      direct[static_cast<std::size_t>(j * band.ld + offset)],
                      band.values[band.Index(i, j)]));
      }
    }
    for (asc::extent_t j = 0; j < 2; ++j) {
      const auto actual = rhs.values[rhs.Index(i, j)];
      ASC_DENSE_TEST_CHECK(
          test, SameScalarBytes(direct_rhs[static_cast<std::size_t>(i + n * j)],
                                actual));
      if (failed < 0) {
        // Independent dyadic diagonal solve, not only a provider comparison.
        ASC_DENSE_TEST_EQ(test, actual, Value<T>(i + j + 1));
      }
    }
    if (i < failed || failed < 0) {
      ASC_DENSE_TEST_EQ(test, band.values[band.Index(i, i)], Value<T>(2));
    }
  }
}

template <typename T>
void Driver(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::extent_t n, asc::extent_t kd, asc::DenseBlasTriangle triangle,
            asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
            asc::extent_t failed, bool nan = false) {
  BandData<T> band(n, kd, triangle, layout);
  MakeDiagonalInput(band, failed, nan);
  RhsData<T> rhs(band, 2, rhs_layout);
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < 2; ++j) {
      rhs.values[rhs.Index(i, j)] = Value<T>(4 * (i + j + 1));
    }
  }
  const auto before = band.values;
  const auto rhs_before = rhs.values;
  auto direct = ColumnCopy(band);
  std::vector<T> direct_rhs(static_cast<std::size_t>(n * 2));
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < 2; ++j) {
      direct_rhs[static_cast<std::size_t>(i + n * j)] =
          rhs.values[rhs.Index(i, j)];
    }
  }
  const auto size = static_cast<lapack_int>(n);
  const auto width = static_cast<lapack_int>(kd);
  const auto ld = static_cast<lapack_int>(band.ld);
  const lapack_int nrhs = 2;
  const char uplo = triangle == kUpper ? 'U' : 'L';
  std::array<lapack_int, 3> info{137, 117, 139};
  WithoutAllocation(test, [&] {
    ExpertNative<T>::kDriver(&uplo, &size, &width, &nrhs, direct.data(), &ld,
                             direct_rhs.data(), &size, &info[1], 1);
    return true;
  });
  const auto plan =
      Take(asc::QueryPbsvWorkspace(provider, band.View(), rhs.View()));
  Storage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbsv(provider, band.View(), rhs.View(), plan, storage.View(),
                     report);
  });
  const auto expected_info = failed < 0 || (nan && kd <= 64) ? 0 : failed + 1;
  ASC_DENSE_TEST_EQ(test, info[1], expected_info);
  ASC_DENSE_TEST_EQ(test, info.front(), 137);
  ASC_DENSE_TEST_EQ(test, info.back(), 139);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), info[1]);
  ASC_DENSE_TEST_EQ(test, status.ok(), info[1] == 0);
  CheckDriverOutputs(test, band, rhs, direct, direct_rhs, failed);
  if (info[1] > 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), failed);
    ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, rhs.values));
  } else if (nan) {
    // Raw INFO=0 with NaN outputs is explicitly not a mathematical pass.
    ASC_DENSE_TEST_CHECK(
        test, !std::isfinite(std::abs(ToWide(rhs.values[rhs.Index(0, 0)]))));
  }
  band.CheckPadding(test, before);
  storage.Check(test);
}

template <typename T>
void Equilibration(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                   asc::DenseBlasRealType<T> diagonal) {
  using Real = asc::DenseBlasRealType<T>;
  const Real nan = std::numeric_limits<Real>::quiet_NaN();
  BandData<T> band(1, 3, triangle, layout);
  std::fill(band.values.begin(), band.values.end(), Value<T>(nan, nan));
  band.values[band.Index(0, 0)] = Value<T>(diagonal, nan);
  const auto before = band.values;
  auto direct = ColumnCopy(band);
  Real direct_scale = -17;
  Real direct_condition = -19;
  Real direct_maximum = -23;
  const lapack_int n = 1;
  const lapack_int kd = 3;
  const auto ld = static_cast<lapack_int>(band.ld);
  const char uplo = triangle == kUpper ? 'U' : 'L';
  std::array<lapack_int, 3> info{137, 117, 139};
  WithoutAllocation(test, [&] {
    ExpertNative<T>::kEquilibrate(&uplo, &n, &kd, direct.data(), &ld,
                                  &direct_scale, &direct_condition,
                                  &direct_maximum, &info[1], 1);
    return true;
  });
  std::array<Real, 3> scales{Real{-29}, Real{-17}, Real{-31}};
  const auto output = Take(asc::DenseBlasVectorView<Real>::Create(
      &scales[1], 1, 1, {scales.data(), sizeof(scales), kHost}));
  asc::LapackBandEquilibrationStatistics<Real> statistics{Real{-19}, Real{-23}};
  const auto plan = Take(
      asc::QueryPbequWorkspace(provider, band.ConstView(), output, statistics));
  Storage<T> storage(plan);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Pbequ(provider, band.ConstView(), output, statistics, plan,
                      storage.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, info.front(), 137);
  ASC_DENSE_TEST_EQ(test, info.back(), 139);
  ASC_DENSE_TEST_EQ(test, info[1], diagonal <= 0 ? 1 : 0);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), info[1]);
  ASC_DENSE_TEST_EQ(test, status.ok(), info[1] == 0);
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(scales[1], direct_scale));
  ASC_DENSE_TEST_CHECK(
      test, SameScalarBytes(statistics.scale_condition, direct_condition));
  ASC_DENSE_TEST_CHECK(
      test, SameScalarBytes(statistics.absolute_maximum, direct_maximum));
  ASC_DENSE_TEST_EQ(test, scales.front(), Real{-29});
  ASC_DENSE_TEST_EQ(test, scales.back(), Real{-31});
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, band.values));
  if (diagonal > 0 && std::isfinite(diagonal)) {
    // Independent wide arithmetic verifies even a subnormal positive diagonal.
    const long double scaled = static_cast<long double>(scales[1]) * diagonal *
                               static_cast<long double>(scales[1]);
    ASC_DENSE_TEST_CHECK(
        test, std::abs(scaled - 1) <= 8 * std::numeric_limits<Real>::epsilon());
    ASC_DENSE_TEST_EQ(test, statistics.scale_condition, Real{1});
  } else if (!std::isfinite(diagonal)) {
    ASC_DENSE_TEST_CHECK(test, !std::isfinite(statistics.scale_condition));
  }
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const Real diagonal :
           std::array<Real, 7>{Real{4}, Real{0}, Real{-1},
                               std::numeric_limits<Real>::denorm_min(),
                               std::numeric_limits<Real>::max(),
                               std::numeric_limits<Real>::infinity(),
                               std::numeric_limits<Real>::quiet_NaN()}) {
        Equilibration<T>(test, provider, triangle, layout, diagonal);
      }
      for (const auto rhs_layout : {kColumn, kRow}) {
        Driver<T>(test, provider, 6, 2, triangle, layout, rhs_layout, -1);
        Driver<T>(test, provider, 1, 0, triangle, layout, rhs_layout, 0, true);
        for (const asc::extent_t failed : {0, 31, 32, 63, 64}) {
          Driver<T>(test, provider, 136, 65, triangle, layout, rhs_layout,
                    failed);
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
