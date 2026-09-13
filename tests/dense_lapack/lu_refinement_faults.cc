#include "lu_refinement_faults.h"

#include <cstddef>
#include <limits>

#include "lapack_build_config.h"
#define HAVE_LAPACK_CONFIG_H
#define LAPACK_COMPLEX_CPP
#if ASC_LAPACK_INTEGER_BITS == 64
#define LAPACK_ILP64
#endif
#include <lapacke_config.h>

namespace {
asc_lapack_test::RefinementFault g_fault =
    asc_lapack_test::RefinementFault::kNone;
}  // namespace
namespace asc_lapack_test {
void SetRefinementFault(RefinementFault fault) { g_fault = fault; }
}  // namespace asc_lapack_test

// GNU test-only wrapping with the independently probed integer/character ABI.
// This is neither a numerical provider nor a process-wide error handler.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dgerfs_(const char*, const lapack_int*, const lapack_int*,
                    const double*, const lapack_int*, const double*,
                    const lapack_int*, const lapack_int*, const double*,
                    const lapack_int*, double*, const lapack_int*, double*,
                    double*, double*, lapack_int*, lapack_int*, std::size_t);
void __wrap_dgerfs_(const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const double* af,
                    const lapack_int* ldaf, const lapack_int* pivots,
                    const double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* ferr, double* berr,
                    double* work, lapack_int* iwork, lapack_int* info,
                    std::size_t trans_length) {
  if (g_fault == asc_lapack_test::RefinementFault::kNegative) {
    *info = -7;
    return;
  }
  __real_dgerfs_(trans, n, nrhs, a, lda, af, ldaf, pivots, b, ldb, x, ldx, ferr,
                 berr, work, iwork, info, trans_length);
  if (g_fault == asc_lapack_test::RefinementFault::kPositive) {
    *info = 1;
  }
  if (*nrhs > 1) {
    switch (g_fault) {
      case asc_lapack_test::RefinementFault::kNanForward:
        ferr[1] = std::numeric_limits<double>::quiet_NaN();
        break;
      case asc_lapack_test::RefinementFault::kInfiniteBackward:
        berr[1] = std::numeric_limits<double>::infinity();
        break;
      case asc_lapack_test::RefinementFault::kNegativeForward:
        ferr[1] = -1;
        break;
      case asc_lapack_test::RefinementFault::kNegativeBackward:
        berr[1] = -1;
        break;
      default:
        break;
    }
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
