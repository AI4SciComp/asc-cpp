#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_POSITIVE_ROTATION_NUMERICAL_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_POSITIVE_ROTATION_NUMERICAL_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include "rotation_numerical.h"
namespace asc_positive_rotation_numerical {
using asc_rotation_numerical::Bits;
template <typename T>
std::array<T, 44> Values() {
  const auto base = asc_rotation_numerical::Values<T>();
  std::array<T, 44> values{};
  std::copy(base.begin(), base.end(), values.begin());
  const int exponent = (std::numeric_limits<T>::min_exponent - 1 +
                        std::numeric_limits<T>::digits) /
                       2;
  const T small = std::ldexp(T{1}, exponent);
  const T large = T{1} / small;
  std::size_t next = base.size();
  for (const T value :
       {std::nextafter(small, T{0}), small, std::nextafter(small, large),
        std::nextafter(large, T{0}), large,
        std::nextafter(large, std::numeric_limits<T>::max())}) {
    values[next++] = value;
    values[next++] = -value;
  }
  return values;
}
// Independent nonnegative-R convention, with the original rotation equations.
// The dimensioned error scales with the norm, without an additive unit floor.
template <typename T>
bool Accurate(T f, T g, T c, T s, T r) {
  const long double fw = f;
  const long double gw = g;
  const long double cw = c;
  const long double sw = s;
  const long double rw = r;
  const long double norm = std::hypot(fw, gw);
  const long double eps = std::numeric_limits<T>::epsilon();
  const long double quantum = std::numeric_limits<T>::denorm_min();
  if (!std::isfinite(c) || !std::isfinite(s) || std::abs(c) > 1 ||
      std::abs(s) > 1 || r < 0 || std::abs(cw * cw + sw * sw - 1) > 64 * eps) {
    return false;
  }
  if (g == 0) {
    return Bits(c) == Bits(std::copysign(T{1}, f)) && s == 0 &&
           Bits(r) == Bits(std::abs(f));
  }
  if (std::abs(cw - fw / norm) > 64 * eps ||
      std::abs(sw - gw / norm) > 64 * eps ||
      std::abs(-sw * (fw / norm) + cw * (gw / norm)) > 64 * eps) {
    return false;
  }
  if (std::isinf(r)) {
    return norm > std::numeric_limits<T>::max() && r > 0;
  }
  return std::isfinite(r) &&
         std::abs(rw - norm) <= 64 * eps * norm + 4 * quantum &&
         std::abs(cw * (fw / norm) + sw * (gw / norm) - rw / norm) <=
             64 * eps + 4 * quantum / norm;
}
}  // namespace asc_positive_rotation_numerical
#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_POSITIVE_ROTATION_NUMERICAL_H_
