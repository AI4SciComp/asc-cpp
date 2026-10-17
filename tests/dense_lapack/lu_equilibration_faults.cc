#include "lu_equilibration_faults.h"

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_lapack_test::EquilibrationFault g_fault =
    asc_lapack_test::EquilibrationFault::kNone;
}  // namespace
namespace asc_lapack_test {
void SetEquilibrationFault(EquilibrationFault fault) { g_fault = fault; }
}  // namespace asc_lapack_test

// Test-only GNU symbol wrapping, not a provider numerical implementation or
// global error-handler replacement. The actual ABI was separately probed.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {

void __real_dgeequ_(const lapack_int*, const lapack_int*, const double*,
                    const lapack_int*, double*, double*, double*, double*,
                    double*, lapack_int*);
void __wrap_dgeequ_(const lapack_int* m, const lapack_int* n, const double* a,
                    const lapack_int* lda, double* rows, double* columns,
                    double* row_condition, double* column_condition,
                    double* amax, lapack_int* info) {
  if (g_fault == asc_lapack_test::EquilibrationFault::kNegative) {
    *info = -4;
    return;
  }
  __real_dgeequ_(m, n, a, lda, rows, columns, row_condition, column_condition,
                 amax, info);
  if (g_fault == asc_lapack_test::EquilibrationFault::kExcess) {
    *info = *m + *n + 1;
  }
}

void __real_dgeequb_(const lapack_int*, const lapack_int*, const double*,
                     const lapack_int*, double*, double*, double*, double*,
                     double*, lapack_int*);
void __wrap_dgeequb_(const lapack_int* m, const lapack_int* n, const double* a,
                     const lapack_int* lda, double* rows, double* columns,
                     double* row_condition, double* column_condition,
                     double* amax, lapack_int* info) {
  if (g_fault == asc_lapack_test::EquilibrationFault::kNegative) {
    *info = -4;
    return;
  }
  __real_dgeequb_(m, n, a, lda, rows, columns, row_condition, column_condition,
                  amax, info);
  if (g_fault == asc_lapack_test::EquilibrationFault::kExcess) {
    *info = *m + *n + 1;
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
