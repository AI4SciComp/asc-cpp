#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_NATIVE_H_
#include <complex>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_block_solve_test {
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs, const float* a,
                   const lapack_int* lda, const lapack_int* pivots, float* b,
                   const lapack_int* ldb, float* work, lapack_int* info) {
  LAPACK_ssytrs2(uplo, n, nrhs, a, lda, pivots, b, ldb, work, info);
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs, const double* a,
                   const lapack_int* lda, const lapack_int* pivots, double* b,
                   const lapack_int* ldb, double* work, lapack_int* info) {
  LAPACK_dsytrs2(uplo, n, nrhs, a, lda, pivots, b, ldb, work, info);
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs,
                   const std::complex<float>* a, const lapack_int* lda,
                   const lapack_int* pivots, std::complex<float>* b,
                   const lapack_int* ldb, std::complex<float>* work,
                   lapack_int* info) {
  if (hermitian) {
    LAPACK_chetrs2(uplo, n, nrhs, a, lda, pivots, b, ldb, work, info);
  } else {
    LAPACK_csytrs2(uplo, n, nrhs, a, lda, pivots, b, ldb, work, info);
  }
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, const lapack_int* nrhs,
                   const std::complex<double>* a, const lapack_int* lda,
                   const lapack_int* pivots, std::complex<double>* b,
                   const lapack_int* ldb, std::complex<double>* work,
                   lapack_int* info) {
  if (hermitian) {
    LAPACK_zhetrs2(uplo, n, nrhs, a, lda, pivots, b, ldb, work, info);
  } else {
    LAPACK_zsytrs2(uplo, n, nrhs, a, lda, pivots, b, ldb, work, info);
  }
}
}  // namespace asc_block_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_NATIVE_H_
