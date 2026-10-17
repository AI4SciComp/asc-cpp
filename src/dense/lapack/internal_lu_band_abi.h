#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_BAND_ABI_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_BAND_ABI_H_

#include "internal_indefinite.h"  // IWYU pragma: export

// GBTF2 is absent from pinned lapack.h. These private declarations follow
// the exact SRC/[sdcz]gbtf2.f GFortran 11.4 -fc-prototypes-external emissions,
// including -fdefault-integer-8 for ILP64. There is no CHARACTER argument or
// hidden length. The existing audited INTEGER and C++ complex ABI applies.
extern "C" {
void LAPACK_GLOBAL_SUFFIX(sgbtf2, SGBTF2)(lapack_int* m, lapack_int* n,
                                          lapack_int* kl, lapack_int* ku,
                                          float* ab, lapack_int* ldab,
                                          lapack_int* ipiv, lapack_int* info);
void LAPACK_GLOBAL_SUFFIX(dgbtf2, DGBTF2)(lapack_int* m, lapack_int* n,
                                          lapack_int* kl, lapack_int* ku,
                                          double* ab, lapack_int* ldab,
                                          lapack_int* ipiv, lapack_int* info);
void LAPACK_GLOBAL_SUFFIX(cgbtf2, CGBTF2)(lapack_int* m, lapack_int* n,
                                          lapack_int* kl, lapack_int* ku,
                                          lapack_complex_float* ab,
                                          lapack_int* ldab, lapack_int* ipiv,
                                          lapack_int* info);
void LAPACK_GLOBAL_SUFFIX(zgbtf2, ZGBTF2)(lapack_int* m, lapack_int* n,
                                          lapack_int* kl, lapack_int* ku,
                                          lapack_complex_double* ab,
                                          lapack_int* ldab, lapack_int* ipiv,
                                          lapack_int* info);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_BAND_ABI_H_
