#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INVERSE_NATIVE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INVERSE_NATIVE_H_
#include <complex>
#include <cstddef>

#include "../../src/dense/lapack/internal_indefinite.h"
#include "../../src/dense/lapack/internal_indefinite_rook_inverse_prototypes.h"
namespace asc_rook_inverse_test {
inline void Native([[maybe_unused]] bool hermitian, char* uplo, lapack_int* n,
                   float* a, lapack_int* lda, lapack_int* pivots, float* work,
                   lapack_int* info) {
  ssytri_rook_(uplo, n, a, lda, pivots, work, info, std::size_t{1});
}
inline void Native([[maybe_unused]] bool hermitian, char* uplo, lapack_int* n,
                   double* a, lapack_int* lda, lapack_int* pivots, double* work,
                   lapack_int* info) {
  dsytri_rook_(uplo, n, a, lda, pivots, work, info, std::size_t{1});
}
inline void Native([[maybe_unused]] bool hermitian, char* uplo, lapack_int* n,
                   std::complex<float>* a, lapack_int* lda, lapack_int* pivots,
                   std::complex<float>* work, lapack_int* info) {
  const auto call = hermitian ? chetri_rook_ : csytri_rook_;
  call(uplo, n, a, lda, pivots, work, info, std::size_t{1});
}
inline void Native([[maybe_unused]] bool hermitian, char* uplo, lapack_int* n,
                   std::complex<double>* a, lapack_int* lda, lapack_int* pivots,
                   std::complex<double>* work, lapack_int* info) {
  const auto call = hermitian ? zhetri_rook_ : zsytri_rook_;
  call(uplo, n, a, lda, pivots, work, info, std::size_t{1});
}
}  // namespace asc_rook_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INVERSE_NATIVE_H_
