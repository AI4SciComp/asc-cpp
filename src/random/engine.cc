#include "asc/random/engine.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/status.h"

namespace asc {

namespace internal_random_engine {

constexpr std::uint32_t kMultiplier0 = 0xd2511f53U;
constexpr std::uint32_t kMultiplier1 = 0xcd9e8d57U;
constexpr std::uint32_t kWeyl0 = 0x9e3779b9U;
constexpr std::uint32_t kWeyl1 = 0xbb67ae85U;

Philox4x32Result Round(Philox4x32Result counter, Philox4x32Key key) noexcept {
  const std::uint64_t product0 =
      static_cast<std::uint64_t>(kMultiplier0) * counter[0];
  const std::uint64_t product1 =
      static_cast<std::uint64_t>(kMultiplier1) * counter[2];

  return {
      static_cast<std::uint32_t>(product1 >> 32U) ^ counter[1] ^ key.words[0],
      static_cast<std::uint32_t>(product1),
      static_cast<std::uint32_t>(product0 >> 32U) ^ counter[3] ^ key.words[1],
      static_cast<std::uint32_t>(product0),
  };
}

Philox4x32Key BumpKey(Philox4x32Key key) noexcept {
  key.words[0] += kWeyl0;
  key.words[1] += kWeyl1;
  return key;
}

}  // namespace internal_random_engine

Philox4x32Result Philox4x32_10(Philox4x32Counter counter,
                               Philox4x32Key key) noexcept {
  Philox4x32Result result = counter.words;
  for (int round = 0; round < 10; ++round) {
    result = internal_random_engine::Round(result, key);
    if (round != 9) {
      key = internal_random_engine::BumpKey(key);
    }
  }
  return result;
}

Philox4x32Result Philox4x32Block(RandomStream stream,
                                 RandomSubsequence subsequence,
                                 std::uint64_t block) noexcept {
  const Philox4x32Counter counter{
      .words = {
          static_cast<std::uint32_t>(block),
          static_cast<std::uint32_t>(block >> 32U),
          static_cast<std::uint32_t>(subsequence),
          static_cast<std::uint32_t>(subsequence >> 32U),
      }};
  const Philox4x32Key key{.words = {
                              static_cast<std::uint32_t>(stream),
                              static_cast<std::uint32_t>(stream >> 32U),
                          }};
  return Philox4x32_10(counter, key);
}

std::uint32_t Philox4x32Word(RandomStream stream, RandomSubsequence subsequence,
                             RandomOffset offset) noexcept {
  const Philox4x32Result block =
      Philox4x32Block(stream, subsequence, offset / 4U);
  return block[static_cast<std::size_t>(offset % 4U)];
}

Result<RandomOffset> AdvanceRandomOffset(RandomOffset offset,
                                         std::uint64_t word_count) noexcept {
  if (word_count > std::numeric_limits<RandomOffset>::max() - offset) {
    return Status(ErrorCode::kOverflow, "Random word offset overflow");
  }
  return offset + word_count;
}

}  // namespace asc
