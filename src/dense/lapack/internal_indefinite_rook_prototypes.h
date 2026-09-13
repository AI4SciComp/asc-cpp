#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_PROTOTYPES_H_

#include <cstddef>
#include <type_traits>

#include "internal_indefinite.h"

// The pinned public lapack.h omits these six source procedures. These private
// declarations are derived from GNU Fortran 11.4.0 -fc-prototypes-external on
// commit 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, separately with the actual
// LP64 flags and ILP64 -fdefault-integer-8 flags. In both emissions CHARACTER
// has a trailing size_t length and the external names have one underscore.
// The audited GNU/Linux provider boundary is narrower than portable Fortran.
// Emitted declarations and executed direct ABI probes are preserved externally
// under continuation-20260912-01/rook-prerequisite-01; no new Fortran
// compilation path is introduced.
#if ASC_LAPACK_INTEGER_BITS == 32
static_assert(std::is_same_v<lapack_int, int>);
#else
// Match the emitted C primitive, not merely an integer typedef of equal width.
// NOLINTNEXTLINE(google-runtime-int)
static_assert(std::is_same_v<lapack_int, long>);
#endif

extern "C" {
void ssytf2_rook_(char* uplo, lapack_int* n, float* matrix, lapack_int* lda,
                  lapack_int* pivots, lapack_int* info,
                  std::size_t uplo_length);
void dsytf2_rook_(char* uplo, lapack_int* n, double* matrix, lapack_int* lda,
                  lapack_int* pivots, lapack_int* info,
                  std::size_t uplo_length);
void csytf2_rook_(char* uplo, lapack_int* n, lapack_complex_float* matrix,
                  lapack_int* lda, lapack_int* pivots, lapack_int* info,
                  std::size_t uplo_length);
void zsytf2_rook_(char* uplo, lapack_int* n, lapack_complex_double* matrix,
                  lapack_int* lda, lapack_int* pivots, lapack_int* info,
                  std::size_t uplo_length);
void chetf2_rook_(char* uplo, lapack_int* n, lapack_complex_float* matrix,
                  lapack_int* lda, lapack_int* pivots, lapack_int* info,
                  std::size_t uplo_length);
void zhetf2_rook_(char* uplo, lapack_int* n, lapack_complex_double* matrix,
                  lapack_int* lda, lapack_int* pivots, lapack_int* info,
                  std::size_t uplo_length);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_PROTOTYPES_H_
