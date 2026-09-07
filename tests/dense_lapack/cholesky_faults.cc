#include "cholesky_faults.h"

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
asc_cholesky_test::Routine g_routine = asc_cholesky_test::Routine::kPotrf;
asc_cholesky_test::Fault g_fault = asc_cholesky_test::Fault::kNone;
std::size_t g_calls = 0;
bool Inject(asc_cholesky_test::Routine routine, lapack_int n,
            lapack_int* info) {
  ++g_calls;
  if (routine != g_routine || g_fault == asc_cholesky_test::Fault::kNone) {
    return false;
  }
  switch (g_fault) {
    case asc_cholesky_test::Fault::kNegative:
      *info = -4;
      break;
    case asc_cholesky_test::Fault::kMinimum:
      *info = std::numeric_limits<lapack_int>::min();
      break;
    case asc_cholesky_test::Fault::kExcess:
      *info = n + 1;
      break;
    default:
      break;
  }
  return true;
}
}  // namespace
namespace asc_cholesky_test {
void SetFault(Routine routine, Fault fault) {
  g_routine = routine;
  g_fault = fault;
}
std::size_t Calls() { return g_calls; }
}  // namespace asc_cholesky_test

// Test-only GNU wrapping of the independently probed trailing CHARACTER length.
// It is neither an alternate production provider nor a global XERBLA handler.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_dpotrf_(const char*, const lapack_int*, double*, const lapack_int*,
                    lapack_int*, std::size_t);
void __wrap_dpotrf_(const char* triangle, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* info,
                    std::size_t length) {
  if (Inject(asc_cholesky_test::Routine::kPotrf, *n, info)) {
    a[0] = -881;
    return;
  }
  __real_dpotrf_(triangle, n, a, lda, info, length);
}
void __real_dpotrf2_(const char*, const lapack_int*, double*, const lapack_int*,
                     lapack_int*, std::size_t);
void __wrap_dpotrf2_(const char* triangle, const lapack_int* n, double* a,
                     const lapack_int* lda, lapack_int* info,
                     std::size_t length) {
  if (Inject(asc_cholesky_test::Routine::kPotrf2, *n, info)) {
    a[0] = -881;
    return;
  }
  __real_dpotrf2_(triangle, n, a, lda, info, length);
}
void __real_dpotf2_(const char*, const lapack_int*, double*, const lapack_int*,
                    lapack_int*, std::size_t);
void __wrap_dpotf2_(const char* triangle, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* info,
                    std::size_t length) {
  if (Inject(asc_cholesky_test::Routine::kPotf2, *n, info)) {
    a[0] = -881;
    return;
  }
  __real_dpotf2_(triangle, n, a, lda, info, length);
}
void __real_dpotri_(const char*, const lapack_int*, double*, const lapack_int*,
                    lapack_int*, std::size_t);
void __wrap_dpotri_(const char* triangle, const lapack_int* n, double* a,
                    const lapack_int* lda, lapack_int* info,
                    std::size_t length) {
  if (Inject(asc_cholesky_test::Routine::kPotri, *n, info)) {
    a[0] = -881;
    return;
  }
  __real_dpotri_(triangle, n, a, lda, info, length);
}
void __real_dpotrs_(const char*, const lapack_int*, const lapack_int*,
                    const double*, const lapack_int*, double*,
                    const lapack_int*, lapack_int*, std::size_t);
void __wrap_dpotrs_(const char* triangle, const lapack_int* n,
                    const lapack_int* nrhs, const double* a,
                    const lapack_int* lda, double* b, const lapack_int* ldb,
                    lapack_int* info, std::size_t length) {
  if (Inject(asc_cholesky_test::Routine::kPotrs, *n, info)) {
    b[0] = -881;

    return;
  }
  __real_dpotrs_(triangle, n, nrhs, a, lda, b, ldb, info, length);
}
void __real_dposv_(const char*, const lapack_int*, const lapack_int*, double*,
                   const lapack_int*, double*, const lapack_int*, lapack_int*,
                   std::size_t);
void __wrap_dposv_(const char* triangle, const lapack_int* n,
                   const lapack_int* nrhs, double* a, const lapack_int* lda,
                   double* b, const lapack_int* ldb, lapack_int* info,
                   std::size_t length) {
  if (Inject(asc_cholesky_test::Routine::kPosv, *n, info)) {
    b[0] = -881;
    a[0] = -882;
    return;
  }
  __real_dposv_(triangle, n, nrhs, a, lda, b, ldb, info, length);
}
}
// NOLINTEND(bugprone-reserved-identifier)
