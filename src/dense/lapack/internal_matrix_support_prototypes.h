#ifndef ASC_DENSE_LAPACK_INTERNAL_MATRIX_SUPPORT_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_MATRIX_SUPPORT_PROTOTYPES_H_
#include <complex>

#include "internal_tridiagonal.h"
// Absent from installed lapack.h. Exact GNU11 emissions use mutable pointers
// and the actual INTEGER function return in both LP64/global-ILP64.
#define ASC_LAPACK_ilaslc LAPACK_GLOBAL(ilaslc, ILASLC)
extern "C" lapack_int ASC_LAPACK_ilaslc(lapack_int* m, lapack_int* n,
                                        float* matrix, lapack_int* leading);
#define ASC_LAPACK_iladlc LAPACK_GLOBAL(iladlc, ILADLC)
extern "C" lapack_int ASC_LAPACK_iladlc(lapack_int* m, lapack_int* n,
                                        double* matrix, lapack_int* leading);
#define ASC_LAPACK_ilaclc LAPACK_GLOBAL(ilaclc, ILACLC)
extern "C" lapack_int ASC_LAPACK_ilaclc(lapack_int* m, lapack_int* n,
                                        std::complex<float>* matrix,
                                        lapack_int* leading);
#define ASC_LAPACK_ilazlc LAPACK_GLOBAL(ilazlc, ILAZLC)
extern "C" lapack_int ASC_LAPACK_ilazlc(lapack_int* m, lapack_int* n,
                                        std::complex<double>* matrix,
                                        lapack_int* leading);
#define ASC_LAPACK_ilaslr LAPACK_GLOBAL(ilaslr, ILASLR)
extern "C" lapack_int ASC_LAPACK_ilaslr(lapack_int* m, lapack_int* n,
                                        float* matrix, lapack_int* leading);
#define ASC_LAPACK_iladlr LAPACK_GLOBAL(iladlr, ILADLR)
extern "C" lapack_int ASC_LAPACK_iladlr(lapack_int* m, lapack_int* n,
                                        double* matrix, lapack_int* leading);
#define ASC_LAPACK_ilaclr LAPACK_GLOBAL(ilaclr, ILACLR)
extern "C" lapack_int ASC_LAPACK_ilaclr(lapack_int* m, lapack_int* n,
                                        std::complex<float>* matrix,
                                        lapack_int* leading);
#define ASC_LAPACK_ilazlr LAPACK_GLOBAL(ilazlr, ILAZLR)
extern "C" lapack_int ASC_LAPACK_ilazlr(lapack_int* m, lapack_int* n,
                                        std::complex<double>* matrix,
                                        lapack_int* leading);
#endif  // ASC_DENSE_LAPACK_INTERNAL_MATRIX_SUPPORT_PROTOTYPES_H_
