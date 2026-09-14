#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_CONVERSION_PROTOTYPES_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_CONVERSION_PROTOTYPES_H_
#include <complex>
#include <cstddef>

#include "internal_tridiagonal.h"

// LAT2 is absent from the pinned lapack.h. GNU 11.4 external prototype emission
// from both exact sources establishes these private LP64/global ILP64 integer
// and CHARACTER-length signatures. Input const qualification follows the source
// bodies; neither definition writes UPLO, N, A, LDA or LDSA. The maintained ABI
// probe compares every emitted argument after removing pointer constness.
#define LAPACK_dlat2s_base LAPACK_GLOBAL(dlat2s, DLAT2S)
#define LAPACK_zlat2c_base LAPACK_GLOBAL(zlat2c, ZLAT2C)
extern "C" {
void LAPACK_dlat2s_base(const char* uplo, const lapack_int* n,
                        const double* input, const lapack_int* lda,
                        float* output, const lapack_int* ldout,
                        lapack_int* info, std::size_t uplo_length);
void LAPACK_zlat2c_base(const char* uplo, const lapack_int* n,
                        const std::complex<double>* input,
                        const lapack_int* lda, std::complex<float>* output,
                        const lapack_int* ldout, lapack_int* info,
                        std::size_t uplo_length);
}
#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_CONVERSION_PROTOTYPES_H_
