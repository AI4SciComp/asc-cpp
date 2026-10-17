#ifndef ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_ROTATION_NUMERICAL_H_
#define ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_ROTATION_NUMERICAL_H_

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>

namespace asc_rotation_numerical {
template <typename T>
auto Bits(T value) {
  return std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
}

template <typename T>
std::array<T, 32> Values() {
  const T small = std::numeric_limits<T>::min();
  const T large = std::numeric_limits<T>::max();
  const T rtmin = std::sqrt(small);
  const T rtmax = std::sqrt((T{1} / small) / 2);
  const std::array<T, 16> magnitudes{0,
                                     std::numeric_limits<T>::denorm_min(),
                                     small / 1024,
                                     small,
                                     std::nextafter(rtmin, T{0}),
                                     rtmin,
                                     std::nextafter(rtmin, large),
                                     T{0.125},
                                     1,
                                     3,
                                     std::nextafter(rtmax, T{0}),
                                     rtmax,
                                     std::nextafter(rtmax, large),
                                     large / 4,
                                     std::nextafter(large, T{0}),
                                     large};
  std::array<T, 32> values{};
  for (std::size_t i = 0; i < magnitudes.size(); ++i) {
    values[2 * i] = magnitudes[i];
    values[2 * i + 1] = -magnitudes[i];
  }
  return values;
}

// A wider independent norm and the original two equations certify the real
// rotation. T's least subnormal accounts for unavoidable output quantization;
// there is no additive unit floor on the dimensional residual or r error.
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
  if (!std::isfinite(c) || !std::isfinite(s) || c < 0 || c > 1 ||
      std::abs(cw * cw + sw * sw - 1) > 64 * eps) {
    return false;
  }
  if (g == 0) {
    return c == 1 && s == 0 && Bits(r) == Bits(f);
  }
  const long double sign = f < 0 ? -1 : 1;
  const long double expected_r = sign * norm;
  const long double expected_c = std::abs(fw) / norm;
  const long double expected_s = sign * gw / norm;
  if (std::abs(cw - expected_c) > 64 * eps ||
      std::abs(sw - expected_s) > 64 * eps ||
      std::abs(-sw * (fw / norm) + cw * (gw / norm)) > 64 * eps) {
    return false;
  }
  if (std::isinf(r)) {
    return norm > std::numeric_limits<T>::max() &&
           std::signbit(rw) == std::signbit(expected_r);
  }
  return std::isfinite(r) &&
         std::abs(rw - expected_r) <= 64 * eps * norm + 4 * quantum &&
         std::abs(cw * (fw / norm) + sw * (gw / norm) - rw / norm) <=
             64 * eps + 4 * quantum / norm;
}
}  // namespace asc_rotation_numerical

#endif  // ASC_TESTS_DENSE_LAPACK_INSTALLED_LU_ROTATION_NUMERICAL_H_
