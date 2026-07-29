#include "asc/random/distribution.h"

#include <cstdint>

namespace asc {

template <>
float Uniform01<float>(std::uint32_t word) noexcept {
  constexpr float kScale = 0x1.0p-24F;
  return static_cast<float>(word >> 8U) * kScale;
}

template <>
double Uniform01<double>(std::uint32_t high_word,
                         std::uint32_t low_word) noexcept {
  constexpr double kScale = 0x1.0p-53;
  const std::uint64_t bits = (static_cast<std::uint64_t>(high_word) << 32U) |
                             static_cast<std::uint64_t>(low_word);
  return static_cast<double>(bits >> 11U) * kScale;
}

}  // namespace asc
