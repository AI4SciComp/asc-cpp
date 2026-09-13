#include "positive_tridiagonal_condition_entry.h"

#include <complex>
#include <cstddef>
#include <cstring>
#include <limits>
#include <type_traits>

#include "internal_tridiagonal.h"
namespace asc_ptcon_entry {
namespace {
thread_local std::size_t g_calls = 0;
thread_local Fault g_fault = Fault::kNone;
thread_local void (*g_callback)(void*) = nullptr;
thread_local void* g_argument = nullptr;
template <typename Real>
bool Enter(Real* output, lapack_int* info) {
  ++g_calls;
  if (g_callback != nullptr) {
    g_callback(g_argument);
  }
  switch (g_fault) {
    case Fault::kNone:
      return false;
    case Fault::kNoInfo:
      *output = Real{0.5};
      return true;
    case Fault::kPartialInfo: {
      const lapack_int zero = 0;
      // Deliberately write only the low-address half of the native INTEGER.
      // This Linux x86_64 little-endian fault keeps the high sentinel bit.
      std::memcpy(info, &zero, sizeof(lapack_int) / 2);
      *output = Real{0.5};
      return true;
    }
    case Fault::kNegativeInfo:
      *info = -4;
      *output = Real{0.5};
      return true;
    case Fault::kPositiveInfo:
      *info = 3;
      *output = Real{0.5};
      return true;
    case Fault::kNoOutput:
      *info = 0;
      return true;
    case Fault::kNegativeOutput:
      *info = 0;
      *output = -2;
      return true;
    case Fault::kNanOutput:
      *info = 0;
      *output = std::numeric_limits<Real>::quiet_NaN();
      return true;
    case Fault::kInfiniteOutput:
      *info = 0;
      *output = std::numeric_limits<Real>::infinity();
      return true;
  }
  return false;
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
// Existing ELF interception spellings are private test implementation.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_sptcon) __real_sptcon_;
extern "C" void __wrap_sptcon_(const lapack_int* n, const float* diagonal,
                               const float* off, const float* norm,
                               float* output, float* work, lapack_int* info) {
  if (!Enter(output, info)) {
    __real_sptcon_(n, diagonal, off, norm, output, work, info);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_sptcon), decltype(__wrap_sptcon_)>);
extern "C" decltype(LAPACK_dptcon) __real_dptcon_;
extern "C" void __wrap_dptcon_(const lapack_int* n, const double* diagonal,
                               const double* off, const double* norm,
                               double* output, double* work, lapack_int* info) {
  if (!Enter(output, info)) {
    __real_dptcon_(n, diagonal, off, norm, output, work, info);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_dptcon), decltype(__wrap_dptcon_)>);
extern "C" decltype(LAPACK_cptcon) __real_cptcon_;
extern "C" void __wrap_cptcon_(const lapack_int* n, const float* diagonal,
                               const std::complex<float>* off,
                               const float* norm, float* output, float* work,
                               lapack_int* info) {
  if (!Enter(output, info)) {
    __real_cptcon_(n, diagonal, off, norm, output, work, info);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_cptcon), decltype(__wrap_cptcon_)>);
extern "C" decltype(LAPACK_zptcon) __real_zptcon_;
extern "C" void __wrap_zptcon_(const lapack_int* n, const double* diagonal,
                               const std::complex<double>* off,
                               const double* norm, double* output, double* work,
                               lapack_int* info) {
  if (!Enter(output, info)) {
    __real_zptcon_(n, diagonal, off, norm, output, work, info);
  }
}
static_assert(
    std::is_same_v<decltype(LAPACK_zptcon), decltype(__wrap_zptcon_)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_ptcon_entry
