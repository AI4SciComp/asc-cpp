// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_RANDOM_TYPES_H_
#define ASC_RANDOM_TYPES_H_

#include <cstdint>

namespace asc {

/// @brief Explicit 64-bit key selecting one logical random stream.
struct RandomKey {
  std::uint64_t value = 0;

  friend constexpr bool operator==(RandomKey, RandomKey) noexcept = default;
};

/// @brief Explicit 128-bit logical position in a counter-based stream.
struct RandomCounter {
  std::uint64_t subsequence = 0;
  std::uint64_t offset = 0;

  friend constexpr bool operator==(RandomCounter,
                                   RandomCounter) noexcept = default;
};

}  // namespace asc

#endif  // ASC_RANDOM_TYPES_H_
