#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_ABI_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_ABI_H_

#include <complex>
#include <cstddef>
#include <type_traits>

#include "lapack_build_config.h"  // IWYU pragma: export

#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#elif ASC_LAPACK_INTEGER_BITS != 32
#error "Reference LAPACK requires the audited 32/64-bit integer ABI"
#endif
#include <lapack.h>          // IWYU pragma: export
#include <lapacke_config.h>  // IWYU pragma: export

#if !defined(__GLIBCXX__) || __GLIBCXX__ != 20230528
#error "Reference LAPACK complex ABI requires the audited libstdc++ build"
#endif
#ifndef LAPACK_FORTRAN_STRLEN_END
#error "PBTF2 requires the audited trailing Fortran character-length ABI"
#endif

static_assert(sizeof(lapack_int) * 8 == ASC_LAPACK_INTEGER_BITS);
static_assert(std::is_same_v<lapack_complex_float, std::complex<float>>);
static_assert(std::is_same_v<lapack_complex_double, std::complex<double>>);

// PBTF2 is absent from the pinned lapack.h. These private prototypes are
// derived from each exact SRC/*pbtf2.f by the actual GFortran 11.4 compiler
// with -fc-prototypes-external (and -fdefault-integer-8 for true ILP64).
// The verified C++ complex representation and trailing size_t length are
// retained; see band-cholesky-review.md and the independent ABI probes.
extern "C" {
void LAPACK_GLOBAL_SUFFIX(spbtf2, SPBTF2)(char* uplo, lapack_int* n,
                                          lapack_int* kd, float* ab,
                                          lapack_int* ldab, lapack_int* info,
                                          std::size_t uplo_length);
void LAPACK_GLOBAL_SUFFIX(dpbtf2, DPBTF2)(char* uplo, lapack_int* n,
                                          lapack_int* kd, double* ab,
                                          lapack_int* ldab, lapack_int* info,
                                          std::size_t uplo_length);
void LAPACK_GLOBAL_SUFFIX(cpbtf2,
                          CPBTF2)(char* uplo, lapack_int* n, lapack_int* kd,
                                  lapack_complex_float* ab, lapack_int* ldab,
                                  lapack_int* info, std::size_t uplo_length);
void LAPACK_GLOBAL_SUFFIX(zpbtf2,
                          ZPBTF2)(char* uplo, lapack_int* n, lapack_int* kd,
                                  lapack_complex_double* ab, lapack_int* ldab,
                                  lapack_int* info, std::size_t uplo_length);
}

#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_ABI_H_
