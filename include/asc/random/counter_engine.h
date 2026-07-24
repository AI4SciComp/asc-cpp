// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_RANDOM_COUNTER_ENGINE_H_
#define ASC_RANDOM_COUNTER_ENGINE_H_

#include <array>
#include <cstdint>

#include "asc/core/config.h"
#include "asc/random/types.h"

namespace asc {

/// @brief Random123 Philox4x32 counter engine with exactly ten rounds.
///
/// Results are a pure function of the key and counter. Algorithm word order
/// and constants are versioned by this type name and must not change.
class ASC_EXPORT Philox4x32_10 {
 public:
  using ResultType = std::array<std::uint32_t, 4>;

  /// @brief Generate four words at an explicit logical counter position.
  static ResultType Generate(RandomKey key, RandomCounter counter) noexcept;

  /// @brief Generate one 64-bit word from result words zero and one.
  static std::uint64_t Generate64(RandomKey key,
                                  RandomCounter counter) noexcept;
};

}  // namespace asc

#endif  // ASC_RANDOM_COUNTER_ENGINE_H_
