#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_aasen_driver_test {
template <typename T>
void NativePointers(bool he, const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, T* a, const lapack_int* lda,
                    lapack_int* pivots, T* b, const lapack_int* ldb, T* work,
                    const lapack_int* lwork, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssysv_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                    info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsysv_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                    info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chesv_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      info);
    } else {
      LAPACK_csysv_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhesv_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      info);
    } else {
      LAPACK_zsysv_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                      info);
    }
  }
}
template <typename T>
void Native(bool he, char triangle, lapack_int n, lapack_int nrhs, T* a,
            lapack_int lda, lapack_int* pivots, T* b, lapack_int ldb, T* work,
            lapack_int lwork, lapack_int& info) {
  NativePointers(he, &triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                 &lwork, &info);
}
}  // namespace asc_aasen_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_DRIVER_NATIVE_H_
