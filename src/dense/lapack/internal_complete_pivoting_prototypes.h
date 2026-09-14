#ifndef ASC_DENSE_LAPACK_INTERNAL_COMPLETE_PIVOTING_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_COMPLETE_PIVOTING_PROTOTYPES_H_

#include <complex>

#include "internal_tridiagonal.h"

// GETC2/GESC2 are absent from the pinned lapack.h. These private signatures
// match GNU11 prototype emissions from all eight pinned definitions for both
// actual INTEGER ABIs. No CHARACTER argument or hidden length is present.
#define ASC_LAPACK_sgetc2 LAPACK_GLOBAL(sgetc2, SGETC2)
#define ASC_LAPACK_sgesc2 LAPACK_GLOBAL(sgesc2, SGESC2)
#define ASC_LAPACK_dgetc2 LAPACK_GLOBAL(dgetc2, DGETC2)
#define ASC_LAPACK_dgesc2 LAPACK_GLOBAL(dgesc2, DGESC2)
#define ASC_LAPACK_cgetc2 LAPACK_GLOBAL(cgetc2, CGETC2)
#define ASC_LAPACK_cgesc2 LAPACK_GLOBAL(cgesc2, CGESC2)
#define ASC_LAPACK_zgetc2 LAPACK_GLOBAL(zgetc2, ZGETC2)
#define ASC_LAPACK_zgesc2 LAPACK_GLOBAL(zgesc2, ZGESC2)
extern "C" {
void ASC_LAPACK_sgetc2(const lapack_int* n, float* a, const lapack_int* lda,
                       lapack_int* row_pivots, lapack_int* column_pivots,
                       lapack_int* info);
void ASC_LAPACK_sgesc2(const lapack_int* n, const float* a,
                       const lapack_int* lda, float* rhs,
                       const lapack_int* row_pivots,
                       const lapack_int* column_pivots, float* scale);
void ASC_LAPACK_dgetc2(const lapack_int* n, double* a, const lapack_int* lda,
                       lapack_int* row_pivots, lapack_int* column_pivots,
                       lapack_int* info);
void ASC_LAPACK_dgesc2(const lapack_int* n, const double* a,
                       const lapack_int* lda, double* rhs,
                       const lapack_int* row_pivots,
                       const lapack_int* column_pivots, double* scale);
void ASC_LAPACK_cgetc2(const lapack_int* n, std::complex<float>* a,
                       const lapack_int* lda, lapack_int* row_pivots,
                       lapack_int* column_pivots, lapack_int* info);
void ASC_LAPACK_cgesc2(const lapack_int* n, const std::complex<float>* a,
                       const lapack_int* lda, std::complex<float>* rhs,
                       const lapack_int* row_pivots,
                       const lapack_int* column_pivots, float* scale);
void ASC_LAPACK_zgetc2(const lapack_int* n, std::complex<double>* a,
                       const lapack_int* lda, lapack_int* row_pivots,
                       lapack_int* column_pivots, lapack_int* info);
void ASC_LAPACK_zgesc2(const lapack_int* n, const std::complex<double>* a,
                       const lapack_int* lda, std::complex<double>* rhs,
                       const lapack_int* row_pivots,
                       const lapack_int* column_pivots, double* scale);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_COMPLETE_PIVOTING_PROTOTYPES_H_
