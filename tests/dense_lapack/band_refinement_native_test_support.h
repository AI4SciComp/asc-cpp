#ifndef ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_NATIVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_NATIVE_TEST_SUPPORT_H_

#include <complex>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "band_cholesky_test_support.h"
#include "band_refinement_test_support.h"

namespace asc_band_refinement_test {
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kCall = LAPACK_spbrfs_base;
};
template <>
struct Native<double> {
  static constexpr auto kCall = LAPACK_dpbrfs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kCall = LAPACK_cpbrfs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kCall = LAPACK_zpbrfs_base;
};

template <typename T>
std::vector<T> ColumnBand(const asc_band_test::BandData<T>& band,
                          bool hermitian) {
  const auto n = static_cast<std::size_t>(band.n);
  const auto kd = static_cast<std::size_t>(band.kd);
  std::vector<T> values(n * (kd + 1) + 2, asc_band_test::Value<T>(-353, 43));
  for (asc::extent_t j = 0; j < band.n; ++j) {
    for (asc::extent_t i = 0; i < band.n; ++i) {
      if (!band.Selected(i, j)) {
        continue;
      }
      const auto offset =
          band.triangle == asc_band_test::kUpper ? band.kd + i - j : i - j;
      const auto at = 1 + static_cast<std::size_t>(j) * (kd + 1) +
                      static_cast<std::size_t>(offset);
      if constexpr (asc::DenseBlasComplex<T>) {
        if (hermitian && i == j) {
          values[at] = T{band.values[band.Index(i, j)].real(), 0};
          continue;
        }
      }
      values[at] = band.values[band.Index(i, j)];
    }
  }
  return values;
}
template <typename T>
std::vector<T> ColumnRhs(const asc_band_test::RhsData<T>& rhs) {
  const auto n = static_cast<std::size_t>(rhs.n);
  std::vector<T> values(n * static_cast<std::size_t>(rhs.count) + 2,
                        asc_band_test::Value<T>(-359, 47));
  for (asc::extent_t j = 0; j < rhs.count; ++j) {
    for (asc::extent_t i = 0; i < rhs.n; ++i) {
      values[1 + static_cast<std::size_t>(j) * n +
             static_cast<std::size_t>(i)] = rhs.values[rhs.Index(i, j)];
    }
  }
  return values;
}
template <typename T>
struct DirectResult {
  using Real = asc::DenseBlasRealType<T>;
  std::vector<T> solution;
  std::vector<Real> ferr;
  std::vector<Real> berr;
  lapack_int info = -1;
};

template <typename T>
DirectResult<T> Direct(asc_dense_test::TestContext& test,
                       const Fixture<T>& data) {
  using Real = asc::DenseBlasRealType<T>;
  using Auxiliary =
      std::conditional_t<asc::DenseBlasComplex<T>, Real, lapack_int>;
  auto a = ColumnBand(data.a, true);
  auto af = ColumnBand(data.af, false);
  auto b = ColumnRhs(data.b);
  const auto a_before = a;
  const auto af_before = af;
  const auto b_before = b;
  const auto n = static_cast<lapack_int>(data.a.n);
  const auto kd = static_cast<lapack_int>(data.a.kd);
  const auto nrhs = static_cast<lapack_int>(data.b.count);
  const lapack_int band_ld = kd + 1;
  const lapack_int rhs_ld = n == 0 ? 1 : n;
  const char triangle = data.a.triangle == asc_band_test::kUpper ? 'U' : 'L';
  DirectResult<T> result{
      ColumnRhs(data.x),
      std::vector<Real>(static_cast<std::size_t>(nrhs) + 2, Real{-367}),
      std::vector<Real>(static_cast<std::size_t>(nrhs) + 2, Real{-373}), -1};
  std::vector<T> work(
      static_cast<std::size_t>(n) * (asc::DenseBlasComplex<T> ? 2 : 3) + 2,
      asc_band_test::Value<T>(-379, 53));
  std::vector<Auxiliary> auxiliary(static_cast<std::size_t>(n) + 2,
                                   Auxiliary{59});
  asc_band_test::WithoutAllocation(test, [&] {
    Native<T>::kCall(&triangle, &n, &kd, &nrhs, a.data() + 1, &band_ld,
                     af.data() + 1, &band_ld, b.data() + 1, &rhs_ld,
                     result.solution.data() + 1, &rhs_ld,
                     result.ferr.data() + 1, result.berr.data() + 1,
                     work.data() + 1, auxiliary.data() + 1, &result.info, 1);
    return 0;
  });
  ASC_DENSE_TEST_CHECK(test, asc_band_test::SameBytes(a_before, a));
  ASC_DENSE_TEST_CHECK(test, asc_band_test::SameBytes(af_before, af));
  ASC_DENSE_TEST_CHECK(test, asc_band_test::SameBytes(b_before, b));
  ASC_DENSE_TEST_EQ(test, work.front(), asc_band_test::Value<T>(-379, 53));
  ASC_DENSE_TEST_EQ(test, work.back(), asc_band_test::Value<T>(-379, 53));
  ASC_DENSE_TEST_EQ(test, auxiliary.front(), Auxiliary{59});
  ASC_DENSE_TEST_EQ(test, auxiliary.back(), Auxiliary{59});
  ASC_DENSE_TEST_EQ(test, result.solution.front(),
                    asc_band_test::Value<T>(-359, 47));
  ASC_DENSE_TEST_EQ(test, result.solution.back(),
                    asc_band_test::Value<T>(-359, 47));
  ASC_DENSE_TEST_EQ(test, result.ferr.front(), Real{-367});
  ASC_DENSE_TEST_EQ(test, result.ferr.back(), Real{-367});
  ASC_DENSE_TEST_EQ(test, result.berr.front(), Real{-373});
  ASC_DENSE_TEST_EQ(test, result.berr.back(), Real{-373});
  return result;
}
}  // namespace asc_band_refinement_test
#endif  // ASC_TESTS_DENSE_LAPACK_BAND_REFINEMENT_NATIVE_TEST_SUPPORT_H_
