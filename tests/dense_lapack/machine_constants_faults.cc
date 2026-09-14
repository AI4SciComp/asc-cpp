#include "machine_constants_faults.h"

#include <array>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "installed_lu/machine_constants_numerical.h"

namespace asc_machine_constants_fault {
namespace {
thread_local Mode g_mode = Mode::kPass;
thread_local std::size_t g_calls = 0;
thread_local std::size_t g_unexpected = 0;

template <typename T, typename Function>
T Observe(Function real, const char* selector, FORTRAN_STRLEN length) {
  ++g_calls;
  std::array<char, 3> input{'#', *selector, '!'};
  const auto before = input;
  const T result = real(&input[1], length);
  bool accurate = false;
  for (const auto fixture : asc_machine_constants_numerical::Cases<T>()) {
    if (fixture.selector == *selector) {
      accurate = asc_machine_constants_numerical::Bits(result) ==
                 asc_machine_constants_numerical::Bits(fixture.expected);
    }
  }
  if (input != before || length != 1 || !accurate) {
    ++g_unexpected;
  }
  const bool negative = *selector == 'M';
  switch (g_mode) {
    case Mode::kPass:
      return result;
    case Mode::kZero:
      return T{0};
    case Mode::kWrongSign:
      return negative ? T{1} : T{-1};
    case Mode::kNan:
      return std::numeric_limits<T>::quiet_NaN();
    case Mode::kInfinite:
      return std::numeric_limits<T>::infinity();
    case Mode::kChangeSelector:
      // The checked adapter supplies a live mutable private selector copy.
      *const_cast<char*>(selector) = '?';
      return result;
    case Mode::kWrongFinite:
      return negative ? T{-0.5} : T{0.5};
  }
  return result;
}
}  // namespace

void Reset(Mode mode) {
  g_mode = mode;
  g_calls = 0;
  g_unexpected = 0;
}
std::size_t Calls() { return g_calls; }
std::size_t UnexpectedNativeResults() { return g_unexpected; }

// NOLINTBEGIN(bugprone-reserved-identifier)
extern "C" decltype(LAPACK_slamch_base) __real_slamch_;
extern "C" float __wrap_slamch_(const char* selector, FORTRAN_STRLEN length) {
  return Observe<float>(__real_slamch_, selector, length);
}
static_assert(
    std::is_same_v<decltype(__wrap_slamch_), decltype(LAPACK_slamch_base)>);
extern "C" decltype(LAPACK_dlamch_base) __real_dlamch_;
extern "C" double __wrap_dlamch_(const char* selector, FORTRAN_STRLEN length) {
  return Observe<double>(__real_dlamch_, selector, length);
}
static_assert(
    std::is_same_v<decltype(__wrap_dlamch_), decltype(LAPACK_dlamch_base)>);
// NOLINTEND(bugprone-reserved-identifier)
}  // namespace asc_machine_constants_fault
