#ifndef ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_NATIVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_NATIVE_TEST_SUPPORT_H_

#include <complex>

#include "../../src/dense/lapack/internal_band_abi.h"

namespace asc_band_test {
template <typename T>
struct Native;
template <>
struct Native<float> {
  static constexpr auto kFactor = LAPACK_spbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(spbtf2, SPBTF2);
  static constexpr auto kSolve = LAPACK_spbtrs_base;
};
template <>
struct Native<double> {
  static constexpr auto kFactor = LAPACK_dpbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(dpbtf2, DPBTF2);
  static constexpr auto kSolve = LAPACK_dpbtrs_base;
};
template <>
struct Native<std::complex<float>> {
  static constexpr auto kFactor = LAPACK_cpbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(cpbtf2, CPBTF2);
  static constexpr auto kSolve = LAPACK_cpbtrs_base;
};
template <>
struct Native<std::complex<double>> {
  static constexpr auto kFactor = LAPACK_zpbtrf_base;
  static constexpr auto kUnblocked = LAPACK_GLOBAL_SUFFIX(zpbtf2, ZPBTF2);
  static constexpr auto kSolve = LAPACK_zpbtrs_base;
};
}  // namespace asc_band_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_NATIVE_TEST_SUPPORT_H_
