#ifndef ASC_RANDOM_ENGINE_H_
#define ASC_RANDOM_ENGINE_H_

#include <array>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/export.h"

namespace asc {

using Philox4x32Counter = std::array<std::uint32_t, 4>;
using Philox4x32Key = std::array<std::uint32_t, 2>;
using Philox4x32Result = std::array<std::uint32_t, 4>;

using RandomStream = std::uint64_t;
using RandomSubsequence = std::uint64_t;
using RandomOffset = std::uint64_t;

inline constexpr std::uint32_t kRandomSequenceVersion1 = 1;

struct SplitMix64State {
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  std::uint64_t state = 0;

  friend bool operator==(const SplitMix64State&,
                         const SplitMix64State&) = default;
};

class ASC_RANDOM_EXPORT SplitMix64 final {
 public:
  using result_type = std::uint64_t;

  explicit SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  [[nodiscard]] result_type operator()() noexcept;
  [[nodiscard]] SplitMix64State ExportState() const noexcept;
  [[nodiscard]] static Result<SplitMix64> FromState(SplitMix64State state);
  Status RestoreState(SplitMix64State state);

 private:
  std::uint64_t state_;
};

struct Pcg32State {
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  std::uint64_t state = 0;
  std::uint64_t increment = 1;

  friend bool operator==(const Pcg32State&, const Pcg32State&) = default;
};

class ASC_RANDOM_EXPORT Pcg32 final {
 public:
  using result_type = std::uint32_t;

  explicit Pcg32(std::uint64_t seed) noexcept;
  Pcg32(std::uint64_t initial_state, std::uint64_t stream) noexcept;

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  [[nodiscard]] result_type operator()() noexcept;
  [[nodiscard]] Pcg32State ExportState() const noexcept;
  [[nodiscard]] static Result<Pcg32> FromState(Pcg32State state);
  Status RestoreState(Pcg32State state);

 private:
  std::uint64_t state_ = 0;
  std::uint64_t increment_ = 1;
};

struct Xoroshiro64StarState {
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  std::uint32_t first = 0;
  std::uint32_t second = 0;

  friend bool operator==(const Xoroshiro64StarState&,
                         const Xoroshiro64StarState&) = default;
};

class ASC_RANDOM_EXPORT Xoroshiro64Star final {
 public:
  using result_type = std::uint32_t;

  explicit Xoroshiro64Star(std::uint64_t seed) noexcept;

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  [[nodiscard]] result_type operator()() noexcept;
  void Jump() noexcept;
  void LongJump() noexcept;
  [[nodiscard]] Xoroshiro64StarState ExportState() const noexcept;
  [[nodiscard]] static Result<Xoroshiro64Star> FromState(
      Xoroshiro64StarState state);
  Status RestoreState(Xoroshiro64StarState state);

 private:
  std::array<std::uint32_t, 2> state_;
};

struct Xoroshiro128PlusState {
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  std::uint64_t first = 0;
  std::uint64_t second = 0;

  friend bool operator==(const Xoroshiro128PlusState&,
                         const Xoroshiro128PlusState&) = default;
};

class ASC_RANDOM_EXPORT Xoroshiro128Plus final {
 public:
  using result_type = std::uint64_t;

  explicit Xoroshiro128Plus(std::uint64_t seed) noexcept;

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  [[nodiscard]] result_type operator()() noexcept;
  void Jump() noexcept;
  void LongJump() noexcept;
  [[nodiscard]] Xoroshiro128PlusState ExportState() const noexcept;
  [[nodiscard]] static Result<Xoroshiro128Plus> FromState(
      Xoroshiro128PlusState state);
  Status RestoreState(Xoroshiro128PlusState state);

 private:
  std::array<std::uint64_t, 2> state_;
};

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
