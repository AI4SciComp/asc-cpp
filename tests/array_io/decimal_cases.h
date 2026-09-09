#ifndef ASC_TESTS_ARRAY_IO_DECIMAL_CASES_H_
#define ASC_TESTS_ARRAY_IO_DECIMAL_CASES_H_

#include <array>
#include <bit>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace asc_decimal_test {

// Independent IEEE encodings paired with canonical 9/17-digit decimals.
// Includes both zeros, least subnormal, least normal, greatest finite,
// the successor of one, rounded one tenth, and the general-format boundary.
// No ASC formatter or parser constructs these expected strings or bits.
template <typename Real>
struct Cases;
template <>
struct Cases<float> {
  using Bits = std::uint32_t;
  static constexpr std::array<Bits, 8> kBits{0,          0x80000000, 1,
                                             0x00800000, 0x7f7fffff, 0x3f800001,
                                             0x3dcccccd, 0x38d00000};
  static constexpr std::array<std::string_view, 8> kTokens{"0",
                                                           "-0",
                                                           "1.40129846e-45",
                                                           "1.17549435e-38",
                                                           "3.40282347e+38",
                                                           "1.00000012",
                                                           "0.100000001",
                                                           "9.91821289e-05"};
};
template <>
struct Cases<double> {
  using Bits = std::uint64_t;
  static constexpr std::array<Bits, 8> kBits{0,
                                             0x8000000000000000ULL,
                                             1,
                                             0x0010000000000000ULL,
                                             0x7fefffffffffffffULL,
                                             0x3ff0000000000001ULL,
                                             0x3fb999999999999aULL,
                                             0x3f1a000000000000ULL};
  static constexpr std::array<std::string_view, 8> kTokens{
      "0",
      "-0",
      "4.9406564584124654e-324",
      "2.2250738585072014e-308",
      "1.7976931348623157e+308",
      "1.0000000000000002",
      "0.10000000000000001",
      "9.918212890625e-05"};
};

template <typename T>
std::array<T, 8> Values() {
  std::array<T, 8> values{};
  for (std::size_t i = 0; i < values.size(); ++i) {
    if constexpr (std::is_floating_point_v<T>) {
      values[i] = std::bit_cast<T>(Cases<T>::kBits[i]);
    } else {
      using Real = typename T::value_type;
      values[i] = T{std::bit_cast<Real>(Cases<Real>::kBits[i]),
                    std::bit_cast<Real>(Cases<Real>::kBits[7 - i])};
    }
  }
  return values;
}

template <typename T>
std::string Payload(bool matrix_market) {
  std::string result;
  for (std::size_t i = 0; i < 8; ++i) {
    if constexpr (std::is_floating_point_v<T>) {
      result += Cases<T>::kTokens[i];
    } else {
      using Real = typename T::value_type;
      if (!matrix_market) {
        result += '(';
      }
      result += Cases<Real>::kTokens[i];
      result += matrix_market ? ' ' : ',';
      result += Cases<Real>::kTokens[7 - i];
      if (!matrix_market) {
        result += ')';
      }
    }
    result += '\n';
  }
  return result;
}

template <typename T>
bool SameBits(T left, T right) {
  if constexpr (std::is_floating_point_v<T>) {
    using Bits = typename Cases<T>::Bits;
    return std::bit_cast<Bits>(left) == std::bit_cast<Bits>(right);
  } else {
    return SameBits(left.real(), right.real()) &&
           SameBits(left.imag(), right.imag());
  }
}

}  // namespace asc_decimal_test

#endif  // ASC_TESTS_ARRAY_IO_DECIMAL_CASES_H_
