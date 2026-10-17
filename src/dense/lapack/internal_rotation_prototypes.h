#ifndef ASC_DENSE_LAPACK_INTERNAL_ROTATION_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_ROTATION_PROTOTYPES_H_

#include "internal_tridiagonal.h"

// Absent from pinned lapack.h; exact GNU11 emissions from the pinned real
// definitions agree for LP64 and global ILP64. No INTEGER, CHARACTER, hidden
// length or function return argument is present. All five arguments are real.
#define ASC_LAPACK_slartg LAPACK_GLOBAL(slartg, SLARTG)
#define ASC_LAPACK_dlartg LAPACK_GLOBAL(dlartg, DLARTG)
extern "C" {
void ASC_LAPACK_slartg(float* f, float* g, float* c, float* s, float* r);
void ASC_LAPACK_dlartg(double* f, double* g, double* c, double* s, double* r);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_ROTATION_PROTOTYPES_H_
