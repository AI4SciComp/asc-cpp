#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_CONDITION_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_CONDITION_PROTOTYPES_H_

#include <cstddef>
#include <type_traits>

#include "internal_indefinite.h"

// All six routines are absent from the pinned public lapack.h. These private
// declarations match actual GNU Fortran 11.4.0 -fc-prototypes-external output
// on 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, with LP64 and true ILP64
// -fdefault-integer-8. The admitted boundary is GNU/Linux, not portable
// Fortran. Emissions and executed probes are retained externally under
// continuation-20260912-01/rook-condition-prerequisite-01. The emitted input
// pointers lack const; the pinned source never modifies A, IPIV or ANORM.
#if ASC_LAPACK_INTEGER_BITS == 32
static_assert(std::is_same_v<lapack_int, int>);
#else
// NOLINTNEXTLINE(google-runtime-int)
static_assert(std::is_same_v<lapack_int, long>);
#endif

extern "C" {
void ssycon_rook_(char* uplo, lapack_int* n, float* matrix, lapack_int* lda,
                  lapack_int* pivots, float* norm, float* condition,
                  float* work, lapack_int* iwork, lapack_int* info,
                  std::size_t uplo_length);
void dsycon_rook_(char* uplo, lapack_int* n, double* matrix, lapack_int* lda,
                  lapack_int* pivots, double* norm, double* condition,
                  double* work, lapack_int* iwork, lapack_int* info,
                  std::size_t uplo_length);
void csycon_rook_(char* uplo, lapack_int* n, lapack_complex_float* matrix,
                  lapack_int* lda, lapack_int* pivots, float* norm,
                  float* condition, lapack_complex_float* work,
                  lapack_int* info, std::size_t uplo_length);
void zsycon_rook_(char* uplo, lapack_int* n, lapack_complex_double* matrix,
                  lapack_int* lda, lapack_int* pivots, double* norm,
                  double* condition, lapack_complex_double* work,
                  lapack_int* info, std::size_t uplo_length);
void checon_rook_(char* uplo, lapack_int* n, lapack_complex_float* matrix,
                  lapack_int* lda, lapack_int* pivots, float* norm,
                  float* condition, lapack_complex_float* work,
                  lapack_int* info, std::size_t uplo_length);
void zhecon_rook_(char* uplo, lapack_int* n, lapack_complex_double* matrix,
                  lapack_int* lda, lapack_int* pivots, double* norm,
                  double* condition, lapack_complex_double* work,
                  lapack_int* info, std::size_t uplo_length);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_CONDITION_PROTOTYPES_H_
