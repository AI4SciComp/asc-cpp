#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_DRIVER_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_DRIVER_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite_calls.h"
namespace asc_rk_driver_test {
template <typename T>
void Native(bool hermitian, char uplo, lapack_int n, lapack_int nrhs, T* a,
            lapack_int lda, T* e, lapack_int* pivots, T* b, lapack_int ldb,
            T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssysv_rk(&uplo, &n, &nrhs, a, &lda, e, pivots, b, &ldb, work, &lwork,
                    &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsysv_rk(&uplo, &n, &nrhs, a, &lda, e, pivots, b, &ldb, work, &lwork,
                    &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chesv_rk(&uplo, &n, &nrhs, a, &lda, e, pivots, b, &ldb, work,
                      &lwork, &info);
    } else {
      LAPACK_csysv_rk(&uplo, &n, &nrhs, a, &lda, e, pivots, b, &ldb, work,
                      &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      LAPACK_zhesv_rk(&uplo, &n, &nrhs, a, &lda, e, pivots, b, &ldb, work,
                      &lwork, &info);
    } else {
      LAPACK_zsysv_rk(&uplo, &n, &nrhs, a, &lda, e, pivots, b, &ldb, work,
                      &lwork, &info);
    }
  }
}
}  // namespace asc_rk_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_DRIVER_NATIVE_H_
