#ifndef ASC_RANDOM_ENGINE_H_
#define ASC_RANDOM_ENGINE_H_

#include <array>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/random/export.h"

namespace asc {

using Philox4x32Counter = std::array<std::uint32_t, 4>;
using Philox4x32Key = std::array<std::uint32_t, 2>;
using Philox4x32Result = std::array<std::uint32_t, 4>;

using RandomStream = std::uint64_t;
using RandomSubsequence = std::uint64_t;
using RandomOffset = std::uint64_t;

[[nodiscard]] ASC_RANDOM_EXPORT Philox4x32Result
Philox4x32_10(Philox4x32Counter counter, Philox4x32Key key) noexcept;

[[nodiscard]] ASC_RANDOM_EXPORT Philox4x32Result
GeneratePhilox4x32Block(RandomStream stream, RandomSubsequence subsequence,
                        std::uint64_t block) noexcept;

[[nodiscard]] ASC_RANDOM_EXPORT std::uint32_t GeneratePhilox4x32Word(
    RandomStream stream, RandomSubsequence subsequence,
    RandomOffset offset) noexcept;

[[nodiscard]] ASC_RANDOM_EXPORT Result<RandomOffset> AdvanceRandomOffset(
    RandomOffset offset, std::uint64_t word_count);

}  // namespace asc

#endif  // ASC_RANDOM_ENGINE_H_
