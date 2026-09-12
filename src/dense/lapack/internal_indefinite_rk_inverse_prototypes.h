#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_INVERSE_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_INVERSE_PROTOTYPES_H_
#include <complex>
#include <cstddef>
#include <type_traits>

#include "internal_indefinite.h"
// Pinned lapack.h omits TRI_3X. These private declarations match twelve actual
// GNU Fortran 11.4 LP64/ILP64 emissions, including CHARACTER's size_t length,
// at LAPACK commit 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca. Evidence lives in
// continuation-20260912-01/rk-inverse-prerequisite-01. This is the admitted
// GNU/Linux ABI, not portable Fortran. E/IPIV are source input-only.
#if ASC_LAPACK_INTEGER_BITS == 32
static_assert(std::is_same_v<lapack_int, int>);
#else
// NOLINTNEXTLINE(google-runtime-int)
static_assert(std::is_same_v<lapack_int, long>);
#endif
extern "C" {
void ssytri_3x_(char* uplo, lapack_int* n, float* matrix, lapack_int* lda,
                float* off_diagonal, lapack_int* pivots, float* work,
                lapack_int* block_size, lapack_int* info,
                std::size_t uplo_length);
void dsytri_3x_(char* uplo, lapack_int* n, double* matrix, lapack_int* lda,
                double* off_diagonal, lapack_int* pivots, double* work,
                lapack_int* block_size, lapack_int* info,
                std::size_t uplo_length);
void csytri_3x_(char* uplo, lapack_int* n, std::complex<float>* matrix,
                lapack_int* lda, std::complex<float>* off_diagonal,
                lapack_int* pivots, std::complex<float>* work,
                lapack_int* block_size, lapack_int* info,
                std::size_t uplo_length);
void zsytri_3x_(char* uplo, lapack_int* n, std::complex<double>* matrix,
                lapack_int* lda, std::complex<double>* off_diagonal,
                lapack_int* pivots, std::complex<double>* work,
                lapack_int* block_size, lapack_int* info,
                std::size_t uplo_length);
void chetri_3x_(char* uplo, lapack_int* n, std::complex<float>* matrix,
                lapack_int* lda, std::complex<float>* off_diagonal,
                lapack_int* pivots, std::complex<float>* work,
                lapack_int* block_size, lapack_int* info,
                std::size_t uplo_length);
void zhetri_3x_(char* uplo, lapack_int* n, std::complex<double>* matrix,
                lapack_int* lda, std::complex<double>* off_diagonal,
                lapack_int* pivots, std::complex<double>* work,
                lapack_int* block_size, lapack_int* info,
                std::size_t uplo_length);
}
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_INVERSE_PROTOTYPES_H_
