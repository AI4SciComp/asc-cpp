// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_RANDOM_DISTRIBUTION_H_
#define ASC_RANDOM_DISTRIBUTION_H_

#include <concepts>
#include <cstdint>

namespace asc {

namespace detail {

template <typename T>
concept SupportedRandomScalar = std::same_as<T, float> ||
                                std::same_as<T, double>;

}  // namespace detail

/// @brief Stateless mapping from a 64-bit word to the interval [0, 1).
template <detail::SupportedRandomScalar T>
class Uniform01 {
 public:
  using ValueType = T;

  /// @brief Map the most significant value bits without hidden engine state.
  constexpr T operator()(std::uint64_t word) const noexcept {
    if constexpr (std::same_as<T, float>) {
      return static_cast<float>(word >> 40U) * 0x1p-24F;
    } else {
      return static_cast<double>(word >> 11U) * 0x1p-53;
    }
  }
};

}  // namespace asc

#endif  // ASC_RANDOM_DISTRIBUTION_H_
