#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_NATIVE_H_
#include <complex>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_aasen_solve_test {
template <typename T>
void NativePointers(bool he, const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const T* a, const lapack_int* lda,
                    const lapack_int* pivots, T* b, const lapack_int* ldb,
                    T* work, const lapack_int* lwork, lapack_int* info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                     info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrs_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                       info);
    } else {
      LAPACK_csytrs_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                       info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrs_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                       info);
    } else {
      LAPACK_zsytrs_aa(triangle, n, nrhs, a, lda, pivots, b, ldb, work, lwork,
                       info);
    }
  }
}
template <typename T>
void Native(bool he, char triangle, lapack_int n, lapack_int nrhs, const T* a,
            lapack_int lda, const lapack_int* pivots, T* b, lapack_int ldb,
            T* work, lapack_int lwork, lapack_int& info) {
  NativePointers(he, &triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                 &lwork, &info);
}
}  // namespace asc_aasen_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_NATIVE_H_
