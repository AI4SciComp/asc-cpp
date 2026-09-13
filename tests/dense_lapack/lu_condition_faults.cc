#include "lu_condition_faults.h"

#include <cstddef>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_lapack_test::ConditionFault g_fault =
    asc_lapack_test::ConditionFault::kNone;
}  // namespace
namespace asc_lapack_test {
void SetConditionFault(ConditionFault fault) { g_fault = fault; }
}  // namespace asc_lapack_test

// Test-only GNU wrapping, with the independently probed trailing character
// length, not an alternate numerical provider or process-wide error handler.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dgecon_(const char*, const lapack_int*, const double*,
                    const lapack_int*, const double*, double*, double*,
                    lapack_int*, lapack_int*, std::size_t);
void __wrap_dgecon_(const char* norm, const lapack_int* n, const double* a,
                    const lapack_int* lda, const double* anorm, double* rcond,
                    double* work, lapack_int* iwork, lapack_int* info,
                    std::size_t norm_length) {
  if (g_fault == asc_lapack_test::ConditionFault::kNegative) {
    *info = -4;
    return;
  }
  __real_dgecon_(norm, n, a, lda, anorm, rcond, work, iwork, info, norm_length);
  if (g_fault == asc_lapack_test::ConditionFault::kExcess) {
    *info = 2;
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
