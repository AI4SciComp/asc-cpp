#include "scalar_norm_faults.h"

#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
namespace asc_scalar_norm_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;
template <typename T>
T Inject(T actual, T expected) {
  ++g_calls;
  if (actual != expected) {
    ++g_unexpected;
  }
  switch (g_mode) {
    case Mode::kZero:
      return 0;
    case Mode::kNegativeZero:
      return -T{0};
    case Mode::kNegative:
      return -1;
    case Mode::kTiny:
      return std::numeric_limits<T>::denorm_min();
    case Mode::kMaximum:
      return std::numeric_limits<T>::max();
    case Mode::kInfinity:
      return std::numeric_limits<T>::infinity();
    case Mode::kNegativeInfinity:
      return -std::numeric_limits<T>::infinity();
    case Mode::kNan:
      return std::numeric_limits<T>::quiet_NaN();
    case Mode::kPass:
      return actual;
  }
  return actual;
}
}  // namespace
void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_unexpected = 0;
}
std::size_t Calls() { return g_calls; }
std::size_t UnexpectedNativeResults() { return g_unexpected; }
// Only the established GNU linker wrapper names require reserved identifiers.
// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slapy2) __real_slapy2_;
extern "C" lapack_float_return __wrap_slapy2_(const float* x, const float* y) {
  return Inject(__real_slapy2_(x, y), lapack_float_return{5});
}
static_assert(
    std::is_same_v<decltype(__wrap_slapy2_), decltype(LAPACK_slapy2)>);
extern "C" decltype(LAPACK_dlapy2) __real_dlapy2_;
extern "C" double __wrap_dlapy2_(const double* x, const double* y) {
  return Inject(__real_dlapy2_(x, y), 5.0);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlapy2_), decltype(LAPACK_dlapy2)>);
extern "C" decltype(LAPACK_slapy3) __real_slapy3_;
extern "C" lapack_float_return __wrap_slapy3_(const float* x, const float* y,
                                              const float* z) {
  return Inject(__real_slapy3_(x, y, z), lapack_float_return{13});
}
static_assert(
    std::is_same_v<decltype(__wrap_slapy3_), decltype(LAPACK_slapy3)>);
extern "C" decltype(LAPACK_dlapy3) __real_dlapy3_;
extern "C" double __wrap_dlapy3_(const double* x, const double* y,
                                 const double* z) {
  return Inject(__real_dlapy3_(x, y, z), 13.0);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlapy3_), decltype(LAPACK_dlapy3)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_scalar_norm_fault
