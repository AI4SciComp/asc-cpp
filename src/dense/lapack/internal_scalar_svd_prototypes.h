#ifndef ASC_DENSE_LAPACK_INTERNAL_SCALAR_SVD_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_SCALAR_SVD_PROTOTYPES_H_
#include "internal_tridiagonal.h"
// Absent from pinned lapack.h. Actual GNU11 LP64/global-ILP64 emissions
// agree: nine mutable real scalar pointers, no hidden or integer arguments.
#define ASC_LAPACK_slasv2 LAPACK_GLOBAL(slasv2, SLASV2)
#define ASC_LAPACK_dlasv2 LAPACK_GLOBAL(dlasv2, DLASV2)
extern "C" {
void ASC_LAPACK_slasv2(float* f, float* g, float* h, float* small, float* large,
                       float* sr, float* cr, float* sl, float* cl);
void ASC_LAPACK_dlasv2(double* f, double* g, double* h, double* small,
                       double* large, double* sr, double* cr, double* sl,
                       double* cl);
}
#endif  // ASC_DENSE_LAPACK_INTERNAL_SCALAR_SVD_PROTOTYPES_H_
