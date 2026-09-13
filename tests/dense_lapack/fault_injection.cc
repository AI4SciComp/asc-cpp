#include "fault_injection.h"

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_lapack_test::InjectedFault g_fault = asc_lapack_test::InjectedFault::kNone;
}  // namespace
namespace asc_lapack_test {
void SetInjectedFault(InjectedFault fault) { g_fault = fault; }
}  // namespace asc_lapack_test

// Test-only GNU ELF wrapping of the separately probed pinned mangling. No
// production handler or XERBLA replacement is installed. Injected results are
// defensive report tests, never counted as executed provider numerics.
// Names mandated by GNU ld's --wrap protocol, confined to this test.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dgetrf_(const lapack_int*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, lapack_int*);
void __wrap_dgetrf_(const lapack_int* m, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* pivots,
                    lapack_int* info) {
  if (g_fault == asc_lapack_test::InjectedFault::kNegativeInfo) {
    *info = -4;
    return;
  }
  __real_dgetrf_(m, n, a, lda, pivots, info);
  if (g_fault == asc_lapack_test::InjectedFault::kInvalidPivot && *n > 0) {
    pivots[0] = 0;
  }
  if (g_fault == asc_lapack_test::InjectedFault::kExcessInfo) {
    *info = *m + 1;
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
