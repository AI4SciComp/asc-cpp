#ifndef ASC_RANDOM_DISTRIBUTION_H_
#define ASC_RANDOM_DISTRIBUTION_H_

#include <cstdint>

#include "asc/random/export.h"

namespace asc {

template <typename Real>
Real Uniform01(std::uint32_t word) noexcept = delete;

template <>
[[nodiscard]] ASC_RANDOM_EXPORT float Uniform01<float>(
    std::uint32_t word) noexcept;

template <typename Real>
Real Uniform01(std::uint32_t high_word,
               std::uint32_t low_word) noexcept = delete;

template <>
[[nodiscard]] ASC_RANDOM_EXPORT double Uniform01<double>(
    std::uint32_t high_word, std::uint32_t low_word) noexcept;

}  // namespace asc

#endif  // ASC_RANDOM_DISTRIBUTION_H_
