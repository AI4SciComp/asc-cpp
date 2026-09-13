#ifndef ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_NATIVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_NATIVE_TEST_SUPPORT_H_

#include <complex>
#include <cstddef>
#include <type_traits>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_expert_test_support.h"
#include "band_refinement_native_test_support.h"

namespace asc_band_driver_test {
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kCall = LAPACK_spbsvx_base;
};
template <>
struct Native<double> {
  static constexpr auto kCall = LAPACK_dpbsvx_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kCall = LAPACK_cpbsvx_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kCall = LAPACK_zpbsvx_base;
};

template <typename T>
struct DirectResult {
  using Real = asc::DenseBlasRealType<T>;
  std::vector<T> a;
  std::vector<T> af;
  std::vector<T> b;
  std::vector<T> x;
  std::vector<Real> scales;
  std::vector<Real> ferr;
  std::vector<Real> berr;
  Real reciprocal_condition;
  char equilibration;
  lapack_int info = -1;

  explicit DirectResult(const Fixture<T>& fixture)
      : a(asc_band_refinement_test::ColumnBand(fixture.data.a, true)),
        af(asc_band_refinement_test::ColumnBand(fixture.data.af, false)),
        b(asc_band_refinement_test::ColumnRhs(fixture.data.b)),
        x(asc_band_refinement_test::ColumnRhs(fixture.data.x)),
        scales(fixture.scales),
        ferr(fixture.data.ferr),
        berr(fixture.data.berr),
        reciprocal_condition(fixture.reciprocal_condition),
        equilibration(fixture.equilibration ==
                              asc::LapackCholeskyEquilibration::kDiagonal
                          ? 'Y'
                          : 'N') {}
};

template <typename T>
DirectResult<T> Direct(asc_dense_test::TestContext& test,
                       const Fixture<T>& fixture) {
  using Auxiliary = std::conditional_t<asc::DenseBlasComplex<T>,
                                       asc::DenseBlasRealType<T>, lapack_int>;
  const auto n = static_cast<lapack_int>(fixture.data.a.n);
  const auto kd = static_cast<lapack_int>(fixture.data.a.kd);
  const auto nrhs = static_cast<lapack_int>(fixture.data.b.count);
  const lapack_int band_ld = kd + 1;
  const lapack_int rhs_ld = n == 0 ? 1 : n;
  const char triangle =
      fixture.data.a.triangle == asc_band_test::kUpper ? 'U' : 'L';
  DirectResult<T> result(fixture);
  const auto before = result;
  std::vector<T> work(
      static_cast<std::size_t>(n) * (asc::DenseBlasComplex<T> ? 2 : 3) + 2,
      asc_band_test::Value<T>(-431, 67));
  std::vector<Auxiliary> auxiliary(static_cast<std::size_t>(n) + 2,
                                   Auxiliary{71});
  asc_band_test::WithoutAllocation(test, [&] {
    Native<T>::kCall(&fixture.mode, &triangle, &n, &kd, &nrhs,
                     result.a.data() + 1, &band_ld, result.af.data() + 1,
                     &band_ld, &result.equilibration, result.scales.data() + 1,
                     result.b.data() + 1, &rhs_ld, result.x.data() + 1, &rhs_ld,
                     &result.reciprocal_condition, result.ferr.data() + 1,
                     result.berr.data() + 1, work.data() + 1,
                     auxiliary.data() + 1, &result.info, 1, 1, 1);
    return 0;
  });
  ASC_DENSE_TEST_EQ(test, work.front(), asc_band_test::Value<T>(-431, 67));
  ASC_DENSE_TEST_EQ(test, work.back(), asc_band_test::Value<T>(-431, 67));
  ASC_DENSE_TEST_EQ(test, auxiliary.front(), Auxiliary{71});
  ASC_DENSE_TEST_EQ(test, auxiliary.back(), Auxiliary{71});
  for (const auto member : {&DirectResult<T>::a, &DirectResult<T>::af,
                            &DirectResult<T>::b, &DirectResult<T>::x}) {
    ASC_DENSE_TEST_CHECK(
        test, asc_band_test::SameScalarBytes((result.*member).front(),
                                             (before.*member).front()));
    ASC_DENSE_TEST_CHECK(
        test, asc_band_test::SameScalarBytes((result.*member).back(),
                                             (before.*member).back()));
  }
  for (const auto member : {&DirectResult<T>::scales, &DirectResult<T>::ferr,
                            &DirectResult<T>::berr}) {
    ASC_DENSE_TEST_EQ(test, (result.*member).front(), (before.*member).front());
    ASC_DENSE_TEST_EQ(test, (result.*member).back(), (before.*member).back());
  }
  return result;
}

template <typename T>
void CompareBand(asc_dense_test::TestContext& test,
                 const asc_band_test::BandData<T>& data,
                 const std::vector<T>& direct) {
  for (asc::extent_t j = 0; j < data.n; ++j) {
    for (asc::extent_t i = 0; i < data.n; ++i) {
      if (data.Selected(i, j)) {
        const auto offset =
            data.triangle == asc_band_test::kUpper ? data.kd + i - j : i - j;
        const auto at =
            static_cast<std::size_t>(1 + j * (data.kd + 1) + offset);
        ASC_DENSE_TEST_CHECK(
            test, asc_band_test::SameScalarBytes(data.values[data.Index(i, j)],
                                                 direct[at]));
      }
    }
  }
}
template <typename T>
void CompareRhs(asc_dense_test::TestContext& test,
                const asc_band_test::RhsData<T>& data,
                const std::vector<T>& direct) {
  for (asc::extent_t j = 0; j < data.count; ++j) {
    for (asc::extent_t i = 0; i < data.n; ++i) {
      const auto at = static_cast<std::size_t>(1 + j * data.n + i);
      ASC_DENSE_TEST_CHECK(
          test, asc_band_test::SameScalarBytes(data.values[data.Index(i, j)],
                                               direct[at]));
    }
  }
}
}  // namespace asc_band_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_BAND_EXPERT_NATIVE_TEST_SUPPORT_H_
