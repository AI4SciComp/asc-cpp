#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_INVERSE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_INVERSE_NATIVE_H_
#include <complex>
#include <cstddef>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace asc_inverse_test {
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, float* a, const lapack_int* lda,
                   const lapack_int* pivots, float* work, lapack_int* info) {
  LAPACK_ssytri(uplo, n, a, lda, pivots, work, info);
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, double* a, const lapack_int* lda,
                   const lapack_int* pivots, double* work, lapack_int* info) {
  LAPACK_dsytri(uplo, n, a, lda, pivots, work, info);
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, std::complex<float>* a,
                   const lapack_int* lda, const lapack_int* pivots,
                   std::complex<float>* work, lapack_int* info) {
  if (hermitian) {
    LAPACK_chetri(uplo, n, a, lda, pivots, work, info);
  } else {
    LAPACK_csytri(uplo, n, a, lda, pivots, work, info);
  }
}
inline void Native([[maybe_unused]] bool hermitian, const char* uplo,
                   const lapack_int* n, std::complex<double>* a,
                   const lapack_int* lda, const lapack_int* pivots,
                   std::complex<double>* work, lapack_int* info) {
  if (hermitian) {
    LAPACK_zhetri(uplo, n, a, lda, pivots, work, info);
  } else {
    LAPACK_zsytri(uplo, n, a, lda, pivots, work, info);
  }
}
}  // namespace asc_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_INVERSE_NATIVE_H_
