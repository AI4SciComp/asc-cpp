#include "asc/random/engine.h"

#include <array>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {
namespace internal_random_engine {

constexpr std::uint32_t LowWord(std::uint64_t value) noexcept {
  return static_cast<std::uint32_t>(value);
}

constexpr std::uint32_t HighWord(std::uint64_t value) noexcept {
  return static_cast<std::uint32_t>(value >> 32U);
}

Philox4x32Result PhiloxRound(Philox4x32Counter counter,
                             Philox4x32Key key) noexcept {
  constexpr std::uint64_t kFirstMultiplier = UINT64_C(0xD2511F53);
  constexpr std::uint64_t kSecondMultiplier = UINT64_C(0xCD9E8D57);

  const std::uint64_t first_product =
      kFirstMultiplier * static_cast<std::uint64_t>(counter[0]);
  const std::uint64_t second_product =
      kSecondMultiplier * static_cast<std::uint64_t>(counter[2]);
  return {
      HighWord(second_product) ^ counter[1] ^ key[0], LowWord(second_product),
      HighWord(first_product) ^ counter[3] ^ key[1], LowWord(first_product)};
}

Philox4x32Key BumpKey(Philox4x32Key key) noexcept {
  constexpr std::uint32_t kFirstWeyl = UINT32_C(0x9E3779B9);
  constexpr std::uint32_t kSecondWeyl = UINT32_C(0xBB67AE85);
  return {static_cast<std::uint32_t>(key[0] + kFirstWeyl),
          static_cast<std::uint32_t>(key[1] + kSecondWeyl)};
}

}  // namespace internal_random_engine

Philox4x32Result Philox4x32_10(Philox4x32Counter counter,
                               Philox4x32Key key) noexcept {
  for (int round = 0; round < 10; ++round) {
    counter = internal_random_engine::PhiloxRound(counter, key);
    if (round != 9) {
      key = internal_random_engine::BumpKey(key);
    }
  }
  return counter;
}

Philox4x32Result GeneratePhilox4x32Block(RandomStream stream,
                                         RandomSubsequence subsequence,
                                         std::uint64_t block) noexcept {
  const Philox4x32Counter counter = {
      internal_random_engine::LowWord(block),
      internal_random_engine::HighWord(block),
      internal_random_engine::LowWord(subsequence),
      internal_random_engine::HighWord(subsequence)};
  const Philox4x32Key key = {internal_random_engine::LowWord(stream),
                             internal_random_engine::HighWord(stream)};
  return Philox4x32_10(counter, key);
}

std::uint32_t GeneratePhilox4x32Word(RandomStream stream,
                                     RandomSubsequence subsequence,
                                     RandomOffset offset) noexcept {
  const Philox4x32Result block =
      GeneratePhilox4x32Block(stream, subsequence, offset / 4U);
  return block[static_cast<std::size_t>(offset % 4U)];
}

Result<RandomOffset> AdvanceRandomOffset(RandomOffset offset,
                                         std::uint64_t word_count) {
  if (word_count > std::numeric_limits<RandomOffset>::max() - offset) {
    return Status(ErrorCode::kOverflow, "Random word offset overflow");
  }
  return static_cast<RandomOffset>(offset + word_count);
}

}  // namespace asc
