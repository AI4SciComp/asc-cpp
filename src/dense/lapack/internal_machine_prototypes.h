#ifndef ASC_DENSE_LAPACK_INTERNAL_MACHINE_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_MACHINE_PROTOTYPES_H_

#include "internal_tridiagonal.h"

// The installed pinned lapack.h omits LABAD. Actual GNU11 emissions agree in
// LP64 and global ILP64: two mutable real scalar pointers, no hidden arguments.
#define ASC_LAPACK_slabad LAPACK_GLOBAL(slabad, SLABAD)
#define ASC_LAPACK_dlabad LAPACK_GLOBAL(dlabad, DLABAD)
extern "C" {
void ASC_LAPACK_slabad(float* small, float* large);
void ASC_LAPACK_dlabad(double* small, double* large);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_MACHINE_PROTOTYPES_H_
