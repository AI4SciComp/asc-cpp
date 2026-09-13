#include "positive_tridiagonal_refinement_entry.h"

#include <complex>
#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>

#include "internal_tridiagonal.h"
namespace asc_ptrfs_entry {
namespace {
thread_local std::size_t g_calls = 0;
thread_local Fault g_fault = Fault::kNone;
thread_local void (*g_callback)(void*) = nullptr;
thread_local void* g_argument = nullptr;
template <typename T, typename Real>
bool Enter(lapack_int n, lapack_int nrhs, T* x, lapack_int ldx, Real* ferr,
           Real* berr, lapack_int* info) {
  ++g_calls;
  if (g_callback != nullptr) {
    g_callback(g_argument);
  }
  if (g_fault == Fault::kNone) {
    return false;
  }
  for (lapack_int j = 0; j < nrhs; ++j) {
    for (lapack_int i = 0; i < n; ++i) {
      x[j * ldx + i] = T{7};
    }
    if (g_fault != Fault::kNoFerr || j + 1 < nrhs) {
      ferr[j] = Real{0.25};
    }
    if (g_fault != Fault::kNoBerr || j + 1 < nrhs) {
      berr[j] = Real{0.125};
    }
  }
  if (g_fault == Fault::kNoInfo) {
    return true;
  }
  if (g_fault == Fault::kPartialInfo) {
    const lapack_int zero = 0;
    // Admitted little-endian x86_64: high native INTEGER sentinel remains.
    std::memcpy(info, &zero, sizeof(lapack_int) / 2);
    return true;
  }
  *info = 0;
  switch (g_fault) {
    case Fault::kNegativeInfo:
      *info = -3;
      break;
    case Fault::kPositiveInfo:
      *info = 2;
      break;
    case Fault::kNegativeFerr:
      ferr[nrhs - 1] = -2;
      break;
    case Fault::kNegativeBerr:
      berr[nrhs - 1] = -2;
      break;
    case Fault::kNanFerr:
      ferr[nrhs - 1] = std::numeric_limits<Real>::quiet_NaN();
      break;
    case Fault::kInfiniteBerr:
      berr[nrhs - 1] = std::numeric_limits<Real>::infinity();
      break;
    case Fault::kNanSolution:
      x[(nrhs - 1) * ldx + n - 1] = T{std::numeric_limits<Real>::quiet_NaN()};
      break;
    default:
      break;
  }
  return true;
}
}  // namespace
void Reset() {
  g_calls = 0;
  g_fault = Fault::kNone;
  g_callback = nullptr;
  g_argument = nullptr;
}
void SetFault(Fault fault) { g_fault = fault; }
std::size_t Calls() { return g_calls; }
void OnEntry(void (*callback)(void*), void* argument) {
  g_callback = callback;
  g_argument = argument;
}
// Existing ELF test-only symbol spellings; exact pinned prototypes below.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_sptrfs) __real_sptrfs_;
extern "C" void __wrap_sptrfs_(const lapack_int* n, const lapack_int* nrhs,
                               const float* d, const float* e, const float* df,
                               const float* ef, const float* b,
                               const lapack_int* ldb, float* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               float* work, lapack_int* info) {
  if (!Enter(*n, *nrhs, x, *ldx, ferr, berr, info)) {
    __real_sptrfs_(n, nrhs, d, e, df, ef, b, ldb, x, ldx, ferr, berr, work,
                   info);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_sptrfs), decltype(__wrap_sptrfs_)>);
extern "C" decltype(LAPACK_dptrfs) __real_dptrfs_;
extern "C" void __wrap_dptrfs_(const lapack_int* n, const lapack_int* nrhs,
                               const double* d, const double* e,
                               const double* df, const double* ef,
                               const double* b, const lapack_int* ldb,
                               double* x, const lapack_int* ldx, double* ferr,
                               double* berr, double* work, lapack_int* info) {
  if (!Enter(*n, *nrhs, x, *ldx, ferr, berr, info)) {
    __real_dptrfs_(n, nrhs, d, e, df, ef, b, ldb, x, ldx, ferr, berr, work,
                   info);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_dptrfs), decltype(__wrap_dptrfs_)>);
extern "C" decltype(LAPACK_cptrfs_base) __real_cptrfs_;
extern "C" void __wrap_cptrfs_(const char* uplo, const lapack_int* n,
                               const lapack_int* nrhs, const float* d,
                               const std::complex<float>* e, const float* df,
                               const std::complex<float>* ef,
                               const std::complex<float>* b,
                               const lapack_int* ldb, std::complex<float>* x,
                               const lapack_int* ldx, float* ferr, float* berr,
                               std::complex<float>* work, float* rwork,
                               lapack_int* info, FORTRAN_STRLEN length) {
  if (!Enter(*n, *nrhs, x, *ldx, ferr, berr, info)) {
    __real_cptrfs_(uplo, n, nrhs, d, e, df, ef, b, ldb, x, ldx, ferr, berr,
                   work, rwork, info, length);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_cptrfs_base), decltype(__wrap_cptrfs_)>);
extern "C" decltype(LAPACK_zptrfs_base) __real_zptrfs_;
extern "C" void __wrap_zptrfs_(
    const char* uplo, const lapack_int* n, const lapack_int* nrhs,
    const double* d, const std::complex<double>* e, const double* df,
    const std::complex<double>* ef, const std::complex<double>* b,
    const lapack_int* ldb, std::complex<double>* x, const lapack_int* ldx,
    double* ferr, double* berr, std::complex<double>* work, double* rwork,
    lapack_int* info, FORTRAN_STRLEN length) {
  if (!Enter(*n, *nrhs, x, *ldx, ferr, berr, info)) {
    __real_zptrfs_(uplo, n, nrhs, d, e, df, ef, b, ldb, x, ldx, ferr, berr,
                   work, rwork, info, length);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_zptrfs_base), decltype(__wrap_zptrfs_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_ptrfs_entry
