#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_INVERSE_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_INVERSE_PROTOTYPES_H_

#include <cstddef>
#include <type_traits>

#include "internal_indefinite.h"

// These six symbols are omitted from pinned lapack.h. Declarations match
// actual GNU Fortran 11.4 emissions on LAPACK commit
// 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, including -fdefault-integer-8.
// The admitted boundary is GNU/Linux, not a portable Fortran ABI. Untouched
// LP64/ILP64 emissions live in continuation-20260912-01/
// rook-inverse-prerequisite-01. IPIV is input-only despite the emitted type.
#if ASC_LAPACK_INTEGER_BITS == 32
static_assert(std::is_same_v<lapack_int, int>);
#else
// NOLINTNEXTLINE(google-runtime-int)
static_assert(std::is_same_v<lapack_int, long>);
#endif

extern "C" {
void ssytri_rook_(char* uplo, lapack_int* n, float* matrix, lapack_int* lda,
                  lapack_int* pivots, float* work, lapack_int* info,
                  std::size_t uplo_length);
void dsytri_rook_(char* uplo, lapack_int* n, double* matrix, lapack_int* lda,
                  lapack_int* pivots, double* work, lapack_int* info,
                  std::size_t uplo_length);
void csytri_rook_(char* uplo, lapack_int* n, std::complex<float>* matrix,
                  lapack_int* lda, lapack_int* pivots,
                  std::complex<float>* work, lapack_int* info,
                  std::size_t uplo_length);
void zsytri_rook_(char* uplo, lapack_int* n, std::complex<double>* matrix,
                  lapack_int* lda, lapack_int* pivots,
                  std::complex<double>* work, lapack_int* info,
                  std::size_t uplo_length);
void chetri_rook_(char* uplo, lapack_int* n, std::complex<float>* matrix,
                  lapack_int* lda, lapack_int* pivots,
                  std::complex<float>* work, lapack_int* info,
                  std::size_t uplo_length);
void zhetri_rook_(char* uplo, lapack_int* n, std::complex<double>* matrix,
                  lapack_int* lda, lapack_int* pivots,
                  std::complex<double>* work, lapack_int* info,
                  std::size_t uplo_length);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_INVERSE_PROTOTYPES_H_
