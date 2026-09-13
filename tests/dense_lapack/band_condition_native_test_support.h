#ifndef ASC_TESTS_DENSE_LAPACK_BAND_CONDITION_NATIVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_CONDITION_NATIVE_TEST_SUPPORT_H_

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "band_cholesky_test_support.h"

namespace asc_band_condition_test {
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kCall = LAPACK_spbcon_base;
};
template <>
struct Native<double> {
  static constexpr auto kCall = LAPACK_dpbcon_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kCall = LAPACK_cpbcon_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kCall = LAPACK_zpbcon_base;
};

template <typename T>
asc::DenseBlasRealType<T> Direct(asc_dense_test::TestContext& test,
                                 const asc_band_test::BandData<T>& band,
                                 asc::DenseBlasRealType<T> norm,
                                 lapack_int& info) {
  using Real = asc::DenseBlasRealType<T>;
  using Auxiliary =
      std::conditional_t<asc::DenseBlasComplex<T>, Real, lapack_int>;
  const auto n = static_cast<lapack_int>(band.n);
  const auto kd = static_cast<lapack_int>(band.kd);
  const lapack_int ld = kd + 1;
  const char triangle = band.triangle == asc_band_test::kUpper ? 'U' : 'L';
  std::vector<T> ab(static_cast<std::size_t>(n * ld + 2),
                    asc_band_test::Value<T>(-263, 29));
  for (lapack_int j = 0; j < n; ++j) {
    for (lapack_int i = 0; i < n; ++i) {
      if (band.Selected(i, j)) {
        ab[1 + static_cast<std::size_t>(j) * static_cast<std::size_t>(ld) +
           static_cast<std::size_t>(triangle == 'U' ? kd + i - j : i - j)] =
            band.values[band.Index(i, j)];
      }
    }
  }
  const auto before = ab;
  const lapack_int count = (asc::DenseBlasComplex<T> ? 2 : 3) * n;
  std::vector<T> work(static_cast<std::size_t>(count + 2),
                      asc_band_test::Value<T>(-269, 31));
  std::vector<Auxiliary> auxiliary(static_cast<std::size_t>(n + 2),
                                   Auxiliary{37});
  Real rcond = -271;
  asc_band_test::WithoutAllocation(test, [&] {
    Native<T>::kCall(&triangle, &n, &kd, ab.data() + 1, &ld, &norm, &rcond,
                     work.data() + 1, auxiliary.data() + 1, &info, 1);
    return 0;
  });
  ASC_DENSE_TEST_CHECK(test, asc_band_test::SameBytes(before, ab));
  ASC_DENSE_TEST_EQ(test, work.front(), asc_band_test::Value<T>(-269, 31));
  ASC_DENSE_TEST_EQ(test, work.back(), asc_band_test::Value<T>(-269, 31));
  ASC_DENSE_TEST_EQ(test, auxiliary.front(), Auxiliary{37});
  ASC_DENSE_TEST_EQ(test, auxiliary.back(), Auxiliary{37});
  return rcond;
}

// Independent dyadic L=t*[[1,0],[1,1]], U=L^H. Both encode exactly
// A=t^2*[[1,1],[1,2]]; ||A||_1=3*t^2, ||inv(A)||_1=3/t^2.
template <typename T>
void SetAnalyticFactor(asc_band_test::BandData<T>& band, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  const Real t = std::ldexp(Real{1}, exponent);
  for (std::size_t i = 0; i < band.values.size(); ++i) {
    if (band.selected[i] != 0) {
      band.values[i] = T{t};
    }
  }
}
}  // namespace asc_band_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_BAND_CONDITION_NATIVE_TEST_SUPPORT_H_
