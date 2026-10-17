#include "lu_band_equilibration_radix_faults.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_indefinite.h"
namespace {
using asc_gb_equb_faults::Fault;
Fault g_fault = Fault::kPass;
int g_calls = 0;
bool g_sentinel = false;
bool Intercept(lapack_int maximum, lapack_int* info) {
  ++g_calls;
  g_sentinel = *info == std::numeric_limits<lapack_int>::min();
  if (g_fault == Fault::kPass) {
    return false;
  }
  if (g_fault == Fault::kNoInfo) {
    return true;
  }
  if (g_fault == Fault::kPartialInfo) {
    const std::uint16_t partial = 0;
    std::memcpy(info, &partial, sizeof(partial));
    return true;
  }
  if (g_fault == Fault::kNegative) {
    *info = -4;
  }
  if (g_fault == Fault::kMinimum) {
    *info = std::numeric_limits<lapack_int>::min();
  }
  if (g_fault == Fault::kLargePositive) {
    *info = maximum + 1;
  }
  if (g_fault == Fault::kOne) {
    *info = 1;
  }
  return true;
}
}  // namespace
namespace asc_gb_equb_faults {
void Select(Fault fault) {
  g_fault = fault;
  g_calls = 0;
  g_sentinel = false;
}
int Calls() { return g_calls; }
bool SawInfoSentinel() { return g_sentinel; }
}  // namespace asc_gb_equb_faults
// GNU ld reserves these exact names for diagnostic-only interception.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" {
void __real_sgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     float const* ab, lapack_int const* ldab, float* r,
                     float* c, float* rowcnd, float* colcnd, float* amax,
                     lapack_int* info);
void __wrap_sgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     float const* ab, lapack_int const* ldab, float* r,
                     float* c, float* rowcnd, float* colcnd, float* amax,
                     lapack_int* info) {
  if (!Intercept(*m + *n, info)) {
    __real_sgbequb_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_sgbequb_), decltype(LAPACK_sgbequb)>);
void __real_dgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     double const* ab, lapack_int const* ldab, double* r,
                     double* c, double* rowcnd, double* colcnd, double* amax,
                     lapack_int* info);
void __wrap_dgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     double const* ab, lapack_int const* ldab, double* r,
                     double* c, double* rowcnd, double* colcnd, double* amax,
                     lapack_int* info) {
  if (!Intercept(*m + *n, info)) {
    __real_dgbequb_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_dgbequb_), decltype(LAPACK_dgbequb)>);
void __real_cgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     lapack_complex_float const* ab, lapack_int const* ldab,
                     float* r, float* c, float* rowcnd, float* colcnd,
                     float* amax, lapack_int* info);
void __wrap_cgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     lapack_complex_float const* ab, lapack_int const* ldab,
                     float* r, float* c, float* rowcnd, float* colcnd,
                     float* amax, lapack_int* info) {
  if (!Intercept(*m + *n, info)) {
    __real_cgbequb_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_cgbequb_), decltype(LAPACK_cgbequb)>);
void __real_zgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     lapack_complex_double const* ab, lapack_int const* ldab,
                     double* r, double* c, double* rowcnd, double* colcnd,
                     double* amax, lapack_int* info);
void __wrap_zgbequb_(lapack_int const* m, lapack_int const* n,
                     lapack_int const* kl, lapack_int const* ku,
                     lapack_complex_double const* ab, lapack_int const* ldab,
                     double* r, double* c, double* rowcnd, double* colcnd,
                     double* amax, lapack_int* info) {
  if (!Intercept(*m + *n, info)) {
    __real_zgbequb_(m, n, kl, ku, ab, ldab, r, c, rowcnd, colcnd, amax, info);
    return;
  }
}
static_assert(
    std::is_same_v<decltype(__wrap_zgbequb_), decltype(LAPACK_zgbequb)>);
}
// NOLINTEND(bugprone-reserved-identifier)
