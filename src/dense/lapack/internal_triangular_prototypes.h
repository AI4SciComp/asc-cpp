#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_PROTOTYPES_H_

#include <complex>
#include <cstddef>

#include "internal_indefinite.h"

// TRTI2 is absent from the pinned lapack.h. Exact GNU Fortran 11.4 external
// prototype emission from all four pinned sources (LP64 and true ILP64)
// establishes mutable native integers and trailing size_t CHARACTER lengths.
// The private reference facet verifies std::complex/libstdc++ interoperability.
#define LAPACK_strti2_base LAPACK_GLOBAL(strti2, STRTI2)
#define LAPACK_dtrti2_base LAPACK_GLOBAL(dtrti2, DTRTI2)
#define LAPACK_ctrti2_base LAPACK_GLOBAL(ctrti2, CTRTI2)
#define LAPACK_ztrti2_base LAPACK_GLOBAL(ztrti2, ZTRTI2)

extern "C" {
void LAPACK_strti2_base(char* uplo, char* diag, lapack_int* n, float* a,
                        lapack_int* lda, lapack_int* info, std::size_t uplo_len,
                        std::size_t diag_len);
void LAPACK_dtrti2_base(char* uplo, char* diag, lapack_int* n, double* a,
                        lapack_int* lda, lapack_int* info, std::size_t uplo_len,
                        std::size_t diag_len);
void LAPACK_ctrti2_base(char* uplo, char* diag, lapack_int* n,
                        std::complex<float>* a, lapack_int* lda,
                        lapack_int* info, std::size_t uplo_len,
                        std::size_t diag_len);
void LAPACK_ztrti2_base(char* uplo, char* diag, lapack_int* n,
                        std::complex<double>* a, lapack_int* lda,
                        lapack_int* info, std::size_t uplo_len,
                        std::size_t diag_len);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_PROTOTYPES_H_
