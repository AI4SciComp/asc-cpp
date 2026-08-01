#include "asc/random/engine.h"

#include <array>
#include <bit>
#include <cstddef>
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
  constexpr std::uint64_t kFirstMultiplier = 0xD2511F53ULL;
  constexpr std::uint64_t kSecondMultiplier = 0xCD9E8D57ULL;

  const std::uint64_t first_product =
      kFirstMultiplier * static_cast<std::uint64_t>(counter[0]);
  const std::uint64_t second_product =
      kSecondMultiplier * static_cast<std::uint64_t>(counter[2]);
  return {
      HighWord(second_product) ^ counter[1] ^ key[0], LowWord(second_product),
      HighWord(first_product) ^ counter[3] ^ key[1], LowWord(first_product)};
}

Philox4x32Key BumpKey(Philox4x32Key key) noexcept {
  constexpr std::uint32_t kFirstWeyl = 0x9E3779B9U;
  constexpr std::uint32_t kSecondWeyl = 0xBB67AE85U;
  return {static_cast<std::uint32_t>(key[0] + kFirstWeyl),
          static_cast<std::uint32_t>(key[1] + kSecondWeyl)};
}

}  // namespace internal_random_engine

SplitMix64::result_type SplitMix64::operator()() noexcept {
  state_ += 0x9E3779B97F4A7C15ULL;
  std::uint64_t mixed = state_;
  mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
  return mixed ^ (mixed >> 31U);
}

SplitMix64State SplitMix64::ExportState() const noexcept {
  return {.sequence_version = kRandomSequenceVersion1, .state = state_};
}

Result<SplitMix64> SplitMix64::FromState(SplitMix64State state) {
  SplitMix64 engine(0);
  const Status restored = engine.RestoreState(state);
  if (!restored.ok()) {
    return restored;
  }
  return engine;
}

Status SplitMix64::RestoreState(SplitMix64State state) {
  if (state.sequence_version != kRandomSequenceVersion1) {
    return Status(ErrorCode::kVersion,
                  "SplitMix64 state has an unsupported sequence version");
  }
  state_ = state.state;
  return Status::Ok();
}

Pcg32::Pcg32(std::uint64_t seed) noexcept : Pcg32(seed, 0) {}

Pcg32::Pcg32(std::uint64_t initial_state, std::uint64_t stream) noexcept
    : increment_((stream << 1U) | 1U) {
  static_cast<void>((*this)());
  state_ += initial_state;
  static_cast<void>((*this)());
}

Pcg32::result_type Pcg32::operator()() noexcept {
  constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;
  const std::uint64_t previous = state_;
  state_ = previous * kMultiplier + increment_;
  const std::uint32_t xorshifted =
      static_cast<std::uint32_t>(((previous >> 18U) ^ previous) >> 27U);
  const int rotation = static_cast<int>(previous >> 59U);
  return std::rotr(xorshifted, rotation);
}

Pcg32State Pcg32::ExportState() const noexcept {
  return {.sequence_version = kRandomSequenceVersion1,
          .state = state_,
          .increment = increment_};
}

Result<Pcg32> Pcg32::FromState(Pcg32State state) {
  Pcg32 engine(0);
  const Status restored = engine.RestoreState(state);
  if (!restored.ok()) {
    return restored;
  }
  return engine;
}

Status Pcg32::RestoreState(Pcg32State state) {
  if (state.sequence_version != kRandomSequenceVersion1) {
    return Status(ErrorCode::kVersion,
                  "PCG32 state has an unsupported sequence version");
  }
  if ((state.increment & 1U) == 0) {
    return Status(ErrorCode::kInvalidState,
                  "PCG32 encoded stream increment must be odd");
  }
  state_ = state.state;
  increment_ = state.increment;
  return Status::Ok();
}

Xoroshiro64Star::Xoroshiro64Star(std::uint64_t seed) noexcept {
  SplitMix64 expansion(seed);
  state_[0] = static_cast<std::uint32_t>(expansion());
  state_[1] = static_cast<std::uint32_t>(expansion());
}

Xoroshiro64Star::result_type Xoroshiro64Star::operator()() noexcept {
  const std::uint32_t first = state_[0];
  std::uint32_t second = state_[1];
  const std::uint32_t result = first * 0x9E3779BBU;
  second ^= first;
  state_[0] = std::rotl(first, 26) ^ second ^ (second << 9U);
  state_[1] = std::rotl(second, 13);
  return result;
}

void Xoroshiro64Star::Jump() noexcept {
  constexpr std::array<std::uint32_t, 2> kPolynomial = {0x77FCD1A0U,
                                                        0x4CBF99BDU};
  std::array<std::uint32_t, 2> accumulated{};
  for (const std::uint32_t word : kPolynomial) {
    for (int bit = 0; bit < 32; ++bit) {
      if ((word & (std::uint32_t{1} << bit)) != 0) {
        accumulated[0] ^= state_[0];
        accumulated[1] ^= state_[1];
      }
      static_cast<void>((*this)());
    }
  }
  state_ = accumulated;
}

void Xoroshiro64Star::LongJump() noexcept {
  constexpr std::array<std::uint32_t, 2> kPolynomial = {0x3F1F8B95U,
                                                        0xB4E7E463U};
  std::array<std::uint32_t, 2> accumulated{};
  for (const std::uint32_t word : kPolynomial) {
    for (int bit = 0; bit < 32; ++bit) {
      if ((word & (std::uint32_t{1} << bit)) != 0) {
        accumulated[0] ^= state_[0];
        accumulated[1] ^= state_[1];
      }
      static_cast<void>((*this)());
    }
  }
  state_ = accumulated;
}

Xoroshiro64StarState Xoroshiro64Star::ExportState() const noexcept {
  return {.sequence_version = kRandomSequenceVersion1,
          .first = state_[0],
          .second = state_[1]};
}

Result<Xoroshiro64Star> Xoroshiro64Star::FromState(Xoroshiro64StarState state) {
  Xoroshiro64Star engine(0);
  const Status restored = engine.RestoreState(state);
  if (!restored.ok()) {
    return restored;
  }
  return engine;
}

Status Xoroshiro64Star::RestoreState(Xoroshiro64StarState state) {
  if (state.sequence_version != kRandomSequenceVersion1) {
    return Status(ErrorCode::kVersion,
                  "xoroshiro64* state has an unsupported sequence version");
  }
  if (state.first == 0 && state.second == 0) {
    return Status(ErrorCode::kInvalidState,
                  "xoroshiro64* state must not be all zero");
  }
  state_ = {state.first, state.second};
  return Status::Ok();
}

Xoroshiro128Plus::Xoroshiro128Plus(std::uint64_t seed) noexcept {
  SplitMix64 expansion(seed);
  state_[0] = expansion();
  state_[1] = expansion();
}

Xoroshiro128Plus::result_type Xoroshiro128Plus::operator()() noexcept {
  const std::uint64_t first = state_[0];
  std::uint64_t second = state_[1];
  const std::uint64_t result = first + second;
  second ^= first;
  state_[0] = std::rotl(first, 24) ^ second ^ (second << 16U);
  state_[1] = std::rotl(second, 37);
  return result;
}

void Xoroshiro128Plus::Jump() noexcept {
  constexpr std::array<std::uint64_t, 2> kPolynomial = {0xDF900294D8F554A5ULL,
                                                        0x170865DF4B3201FCULL};
  std::array<std::uint64_t, 2> accumulated{};
  for (const std::uint64_t word : kPolynomial) {
    for (int bit = 0; bit < 64; ++bit) {
      if ((word & (std::uint64_t{1} << bit)) != 0) {
        accumulated[0] ^= state_[0];
        accumulated[1] ^= state_[1];
      }
      static_cast<void>((*this)());
    }
  }
  state_ = accumulated;
}

void Xoroshiro128Plus::LongJump() noexcept {
  constexpr std::array<std::uint64_t, 2> kPolynomial = {0xD2A98B26625EEE7BULL,
                                                        0xDDDF9B1090AA7AC1ULL};
  std::array<std::uint64_t, 2> accumulated{};
  for (const std::uint64_t word : kPolynomial) {
    for (int bit = 0; bit < 64; ++bit) {
      if ((word & (std::uint64_t{1} << bit)) != 0) {
        accumulated[0] ^= state_[0];
        accumulated[1] ^= state_[1];
      }
      static_cast<void>((*this)());
    }
  }
  state_ = accumulated;
}

Xoroshiro128PlusState Xoroshiro128Plus::ExportState() const noexcept {
  return {.sequence_version = kRandomSequenceVersion1,
          .first = state_[0],
          .second = state_[1]};
}

Result<Xoroshiro128Plus> Xoroshiro128Plus::FromState(
    Xoroshiro128PlusState state) {
  Xoroshiro128Plus engine(0);
  const Status restored = engine.RestoreState(state);
  if (!restored.ok()) {
    return restored;
  }
  return engine;
}

Status Xoroshiro128Plus::RestoreState(Xoroshiro128PlusState state) {
  if (state.sequence_version != kRandomSequenceVersion1) {
    return Status(ErrorCode::kVersion,
                  "xoroshiro128+ state has an unsupported sequence version");
  }
  if (state.first == 0 && state.second == 0) {
    return Status(ErrorCode::kInvalidState,
                  "xoroshiro128+ state must not be all zero");
  }
  state_ = {state.first, state.second};
  return Status::Ok();
}

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
