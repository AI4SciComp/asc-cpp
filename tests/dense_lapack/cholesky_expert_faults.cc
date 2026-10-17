#include "cholesky_expert_faults.h"

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
asc_cholesky_expert_test::Routine g_routine =
    asc_cholesky_expert_test::Routine::kPocon;
asc_cholesky_expert_test::Fault g_fault =
    asc_cholesky_expert_test::Fault::kNone;
std::size_t g_calls = 0;

bool Inject(asc_cholesky_expert_test::Routine routine, lapack_int n,
            lapack_int* info) {
  ++g_calls;
  if (routine != g_routine ||
      g_fault == asc_cholesky_expert_test::Fault::kNone) {
    return false;
  }
  switch (g_fault) {
    case asc_cholesky_expert_test::Fault::kNegative:
      *info = -4;
      break;
    case asc_cholesky_expert_test::Fault::kMinimum:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case asc_cholesky_expert_test::Fault::kExcess:
      *info = n + 2;
      break;
    default:
      break;
  }
  return true;
}
}  // namespace
namespace asc_cholesky_expert_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
}
std::size_t Calls() { return g_calls; }
}  // namespace asc_cholesky_expert_test

// Test-only GNU wrapping of the already probed trailing CHARACTER lengths.
// Neither an alternate production provider nor a global XERBLA replacement.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dpocon_(const char* triangle, const lapack_int* n, const double* a,
                    const lapack_int* lda, const double* norm, double* rcond,
                    double* work, lapack_int* iwork, lapack_int* info,
                    std::size_t length);
void __wrap_dpocon_(const char* triangle, const lapack_int* n, const double* a,
                    const lapack_int* lda, const double* norm, double* rcond,
                    double* work, lapack_int* iwork, lapack_int* info,
                    std::size_t length) {
  if (!Inject(asc_cholesky_expert_test::Routine::kPocon, *n, info)) {
    __real_dpocon_(triangle, n, a, lda, norm, rcond, work, iwork, info, length);
  }
}
void __real_dporfs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const double* af,
                    const lapack_int* ldaf, const double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* ferr, double* berr, double* work, lapack_int* iwork,
                    lapack_int* info, std::size_t length);
void __wrap_dporfs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, const double* af,
                    const lapack_int* ldaf, const double* b,
                    const lapack_int* ldb, double* x, const lapack_int* ldx,
                    double* ferr, double* berr, double* work, lapack_int* iwork,
                    lapack_int* info, std::size_t length) {
  if (!Inject(asc_cholesky_expert_test::Routine::kPorfs, *n, info)) {
    __real_dporfs_(triangle, n, nrhs, a, lda, af, ldaf, b, ldb, x, ldx, ferr,
                   berr, work, iwork, info, length);
  }
}
void __real_dposvx_(const char* fact, const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* af, const lapack_int* ldaf, char* equed,
                    double* scales, double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* rcond, double* ferr,
                    double* berr, double* work, lapack_int* iwork,
                    lapack_int* info, std::size_t fact_length,
                    std::size_t triangle_length, std::size_t equed_length);
void __wrap_dposvx_(const char* fact, const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, double* a, const lapack_int* lda,
                    double* af, const lapack_int* ldaf, char* equed,
                    double* scales, double* b, const lapack_int* ldb, double* x,
                    const lapack_int* ldx, double* rcond, double* ferr,
                    double* berr, double* work, lapack_int* iwork,
                    lapack_int* info, std::size_t fact_length,
                    std::size_t triangle_length, std::size_t equed_length) {
  if (!Inject(asc_cholesky_expert_test::Routine::kPosvx, *n, info)) {
    __real_dposvx_(fact, triangle, n, nrhs, a, lda, af, ldaf, equed, scales, b,
                   ldb, x, ldx, rcond, ferr, berr, work, iwork, info,
                   fact_length, triangle_length, equed_length);
  }
}
void __real_dpoequ_(const lapack_int* n, const double* a, const lapack_int* lda,
                    double* scales, double* scond, double* amax,
                    lapack_int* info);
void __wrap_dpoequ_(const lapack_int* n, const double* a, const lapack_int* lda,
                    double* scales, double* scond, double* amax,
                    lapack_int* info) {
  if (!Inject(asc_cholesky_expert_test::Routine::kPoequ, *n, info)) {
    __real_dpoequ_(n, a, lda, scales, scond, amax, info);
  }
}
void __real_dpoequb_(const lapack_int* n, const double* a,
                     const lapack_int* lda, double* scales, double* scond,
                     double* amax, lapack_int* info);
void __wrap_dpoequb_(const lapack_int* n, const double* a,
                     const lapack_int* lda, double* scales, double* scond,
                     double* amax, lapack_int* info) {
  if (!Inject(asc_cholesky_expert_test::Routine::kPoequb, *n, info)) {
    __real_dpoequb_(n, a, lda, scales, scond, amax, info);
  }
}
}
// NOLINTEND(bugprone-reserved-identifier)
