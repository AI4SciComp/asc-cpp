#include "asc/random/distribution.h"

#include <cstdint>
#include <limits>

namespace asc {

static_assert(std::numeric_limits<float>::is_iec559 &&
              std::numeric_limits<float>::radix == 2 &&
              std::numeric_limits<float>::digits == 24);
static_assert(std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::radix == 2 &&
              std::numeric_limits<double>::digits == 53);

template <>
float Uniform01<float>(std::uint32_t word) noexcept {
  const std::uint32_t significand = word >> 8U;
  return static_cast<float>(significand) * 0x1p-24F;
}

template <>
double Uniform01<double>(std::uint32_t high_word,
                         std::uint32_t low_word) noexcept {
  const std::uint64_t bits = (static_cast<std::uint64_t>(high_word) << 32U) |
                             static_cast<std::uint64_t>(low_word);
  const std::uint64_t significand = bits >> 11U;
  return static_cast<double>(significand) * 0x1p-53;
}

}  // namespace asc
