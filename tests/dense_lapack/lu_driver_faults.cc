#include "lu_driver_faults.h"

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
asc_lapack_test::DriverFault g_fault = asc_lapack_test::DriverFault::kNone;
}  // namespace
namespace asc_lapack_test {
void SetDriverFault(DriverFault fault) { g_fault = fault; }
}  // namespace asc_lapack_test

// GNU test-only wrapper; the audited ABI has three trailing size_t lengths.
// No process-wide handler change and no fake numerical implementation.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dgesvx_(const char*, const char*, const lapack_int*,
                    const lapack_int*, double*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, char*, double*, double*,
                    double*, const lapack_int*, double*, const lapack_int*,
                    double*, double*, double*, double*, lapack_int*,
                    lapack_int*, std::size_t, std::size_t, std::size_t);
void __wrap_dgesvx_(const char* fact, const char* trans, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* af, const lapack_int* ldaf, lapack_int* pivots,
                    char* equed, double* r, double* c, double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* rcond, double* ferr, double* berr, double* work,
                    lapack_int* iwork, lapack_int* info,
                    std::size_t fact_length, std::size_t trans_length,
                    std::size_t equed_length) {
  using asc_lapack_test::DriverFault;
  if (g_fault == DriverFault::kNegative) {
    *info = -8;
    return;
  }
  __real_dgesvx_(fact, trans, n, nrhs, a, lda, af, ldaf, pivots, equed, r, c, b,
                 ldb, x, ldx, rcond, ferr, berr, work, iwork, info, fact_length,
                 trans_length, equed_length);
  switch (g_fault) {
    case DriverFault::kExcessiveInfo:
      *info = *n + 2;
      break;
    case DriverFault::kBadPivot:
      if (*n > 0) {
        pivots[0] = 0;
      }
      break;
    case DriverFault::kInvalidEqued:
      *equed = '?';
      break;
    case DriverFault::kChangedEqued:
      *equed = 'R';
      break;
    case DriverFault::kNanRcond:
      *rcond = std::numeric_limits<double>::quiet_NaN();
      break;
    case DriverFault::kNanFerr:
      if (*nrhs > 0) {
        ferr[0] = std::numeric_limits<double>::quiet_NaN();
      }
      break;
    case DriverFault::kNegativeBerr:
      if (*nrhs > 0) {
        berr[0] = -1;
      }
      break;
    case DriverFault::kInfiniteGrowth:
      work[0] = std::numeric_limits<double>::infinity();
      break;
    default:
      break;
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
