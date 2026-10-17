#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_aasen_test {
template <typename T>
void Native(bool hermitian, char uplo, lapack_int n, T* a, lapack_int lda,
            lapack_int* pivots, T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
    } else {
      LAPACK_csytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      LAPACK_zhetrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
    } else {
      LAPACK_zsytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
    }
  }
}
}  // namespace asc_aasen_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_NATIVE_H_
