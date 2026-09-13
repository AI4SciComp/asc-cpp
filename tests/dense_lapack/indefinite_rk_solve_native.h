#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_NATIVE_H_
#include <complex>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_rk_solve_test {
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs, const float* a,
                   const lapack_int* lda, const float* extra,
                   const lapack_int* pivots, float* b, const lapack_int* ldb,
                   lapack_int* info) {
  LAPACK_ssytrs_3(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, info);
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs, const double* a,
                   const lapack_int* lda, const double* extra,
                   const lapack_int* pivots, double* b, const lapack_int* ldb,
                   lapack_int* info) {
  LAPACK_dsytrs_3(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, info);
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs,
                   const std::complex<float>* a, const lapack_int* lda,
                   const std::complex<float>* extra, const lapack_int* pivots,
                   std::complex<float>* b, const lapack_int* ldb,
                   lapack_int* info) {
  if (hermitian) {
    LAPACK_chetrs_3(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, info);
  } else {
    LAPACK_csytrs_3(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, info);
  }
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs,
                   const std::complex<double>* a, const lapack_int* lda,
                   const std::complex<double>* extra, const lapack_int* pivots,
                   std::complex<double>* b, const lapack_int* ldb,
                   lapack_int* info) {
  if (hermitian) {
    LAPACK_zhetrs_3(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, info);
  } else {
    LAPACK_zsytrs_3(uplo, n, nrhs, a, lda, extra, pivots, b, ldb, info);
  }
}
}  // namespace asc_rk_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_NATIVE_H_
