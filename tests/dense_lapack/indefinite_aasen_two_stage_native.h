#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_aasen_two_stage_test {
template <typename T>
void Native(bool he, char triangle, lapack_int n, T* a, lapack_int lda, T* tb,
            lapack_int ltb, lapack_int* pivots, lapack_int* band_pivots,
            T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                            band_pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                            band_pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    } else {
      LAPACK_csytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    } else {
      LAPACK_zsytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    }
  }
}
}  // namespace asc_aasen_two_stage_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_NATIVE_H_
