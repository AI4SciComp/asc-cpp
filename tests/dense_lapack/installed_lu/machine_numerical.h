#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MACHINE_NUMERICAL_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MACHINE_NUMERICAL_H_

#include <array>
#include <bit>
#include <cstddef>
#include <limits>

namespace asc_machine_numerical {
template <typename T>
auto Bits(T value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

template <typename T>
auto Values() {
  const T tiny = std::numeric_limits<T>::denorm_min();
  const T small = std::numeric_limits<T>::min();
  const T large = std::numeric_limits<T>::max();
  const T epsilon = std::numeric_limits<T>::epsilon();
  const T infinity = std::numeric_limits<T>::infinity();
  const T nan = std::numeric_limits<T>::quiet_NaN();
  return std::array<T, 20>{T{0},    -T{0},    tiny,      -tiny,   small,
                           -small,  large,    -large,    epsilon, -epsilon,
                           T{1},    T{-1},    T{2},      T{-2},   T{0.5},
                           T{-0.5}, infinity, -infinity, nan,     -nan};
}
}  // namespace asc_machine_numerical

#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_MACHINE_NUMERICAL_H_
