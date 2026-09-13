#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_NATIVE_H_
#include <complex>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_rook_driver_test {
inline void Native([[maybe_unused]] bool hermitian, char uplo, lapack_int n,
                   lapack_int nrhs, float* a, lapack_int lda,
                   lapack_int* pivots, float* b, lapack_int ldb, float* work,
                   lapack_int lwork, lapack_int& info) {
  LAPACK_ssysv_rook(&uplo, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &lwork,
                    &info);
}
inline void Native([[maybe_unused]] bool hermitian, char uplo, lapack_int n,
                   lapack_int nrhs, double* a, lapack_int lda,
                   lapack_int* pivots, double* b, lapack_int ldb, double* work,
                   lapack_int lwork, lapack_int& info) {
  LAPACK_dsysv_rook(&uplo, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &lwork,
                    &info);
}
inline void Native([[maybe_unused]] bool hermitian, char uplo, lapack_int n,
                   lapack_int nrhs, std::complex<float>* a, lapack_int lda,
                   lapack_int* pivots, std::complex<float>* b, lapack_int ldb,
                   std::complex<float>* work, lapack_int lwork,
                   lapack_int& info) {
  if (hermitian) {
    LAPACK_chesv_rook(&uplo, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &lwork,
                      &info);
  } else {
    LAPACK_csysv_rook(&uplo, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &lwork,
                      &info);
  }
}
inline void Native([[maybe_unused]] bool hermitian, char uplo, lapack_int n,
                   lapack_int nrhs, std::complex<double>* a, lapack_int lda,
                   lapack_int* pivots, std::complex<double>* b, lapack_int ldb,
                   std::complex<double>* work, lapack_int lwork,
                   lapack_int& info) {
  if (hermitian) {
    LAPACK_zhesv_rook(&uplo, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &lwork,
                      &info);
  } else {
    LAPACK_zsysv_rook(&uplo, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &lwork,
                      &info);
  }
}
}  // namespace asc_rook_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_NATIVE_H_
