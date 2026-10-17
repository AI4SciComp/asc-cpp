#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MACHINE_CONSTANTS_NUMERICAL_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MACHINE_CONSTANTS_NUMERICAL_H_

#include <array>
#include <bit>
#include <cstddef>
#include <limits>

#include "asc/dense/providers/lapack_machine.h"

namespace asc_machine_constants_numerical {
template <typename T>
auto Bits(T value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

template <typename T>
struct Case {
  asc::LapackMachineParameter parameter;
  char selector;
  T expected;
};

template <typename T>
auto Cases() {
  using Parameter = asc::LapackMachineParameter;
  static_assert(std::numeric_limits<T>::is_iec559 &&
                std::numeric_limits<T>::radix == 2);
  return std::array<Case<T>, 10>{{
      {Parameter::kRelativePrecision, 'E',
       std::numeric_limits<T>::epsilon() / 2},
      {Parameter::kSafeMinimum, 'S', std::numeric_limits<T>::min()},
      {Parameter::kRadix, 'B', 2},
      {Parameter::kPrecision, 'P', std::numeric_limits<T>::epsilon()},
      {Parameter::kMantissaDigits, 'N', std::numeric_limits<T>::digits},
      {Parameter::kRoundingFlag, 'R', 1},
      {Parameter::kMinimumExponent, 'M', std::numeric_limits<T>::min_exponent},
      {Parameter::kUnderflowThreshold, 'U', std::numeric_limits<T>::min()},
      {Parameter::kMaximumExponent, 'L', std::numeric_limits<T>::max_exponent},
      {Parameter::kOverflowThreshold, 'O', std::numeric_limits<T>::max()},
  }};
}
}  // namespace asc_machine_constants_numerical

#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MACHINE_CONSTANTS_NUMERICAL_H_
