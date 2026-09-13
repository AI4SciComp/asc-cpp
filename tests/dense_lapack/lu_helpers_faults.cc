#include "lu_helpers_faults.h"

#include <cstddef>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_lapack_test::HelperProbe g_probe;
char g_equed = '\0';
}  // namespace
namespace asc_lapack_test {
void ResetHelperProbe(char returned_equed) {
  g_probe = {};
  g_equed = returned_equed;
}
HelperProbe ReadHelperProbe() { return g_probe; }
}  // namespace asc_lapack_test

// GNU test-only wrappers follow compiler-derived declarations. They do not
// replace a numerical algorithm or install a provider error handler.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dlaswp_(const lapack_int*, double*, const lapack_int*,
                    const lapack_int*, const lapack_int*, const lapack_int*,
                    const lapack_int*);
void __wrap_dlaswp_(const lapack_int* n, double* a, const lapack_int* lda,
                    const lapack_int* k1, const lapack_int* k2,
                    const lapack_int* pivots, const lapack_int* inc) {
  ++g_probe.swap_calls;
  __real_dlaswp_(n, a, lda, k1, k2, pivots, inc);
}
void __real_dlaqge_(lapack_int*, lapack_int*, double*, lapack_int*, double*,
                    double*, double*, double*, double*, char*, std::size_t);
void __wrap_dlaqge_(lapack_int* m, lapack_int* n, double* a, lapack_int* lda,
                    double* rows, double* columns, double* rowcnd,
                    double* colcnd, double* amax, char* equed,
                    std::size_t equed_length) {
  ++g_probe.scale_calls;
  g_probe.equed_length = equed_length;
  __real_dlaqge_(m, n, a, lda, rows, columns, rowcnd, colcnd, amax, equed,
                 equed_length);
  if (g_equed != '\0') {
    *equed = g_equed;
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
