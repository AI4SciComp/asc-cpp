#ifndef ASC_RANDOM_ENGINE_H_
#define ASC_RANDOM_ENGINE_H_

/**
 * @file
 * @brief Public Random engine declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_random_engines
 */

#include <array>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/export.h"

namespace asc {

/**
 * @brief Defines the public Philox4x32Counter type used by this Random engine
 * contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
using Philox4x32Counter = std::array<std::uint32_t, 4>;
/**
 * @brief Defines the public Philox4x32Key type used by this Random engine
 * contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
using Philox4x32Key = std::array<std::uint32_t, 2>;
/**
 * @brief Defines the public Philox4x32Result type used by this Random engine
 * contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
using Philox4x32Result = std::array<std::uint32_t, 4>;

/**
 * @brief Defines the public RandomStream type used by this Random engine
 * contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
using RandomStream = std::uint64_t;
/**
 * @brief Defines the public RandomSubsequence type used by this Random engine
 * contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
using RandomSubsequence = std::uint64_t;
/**
 * @brief Defines the public RandomOffset type used by this Random engine
 * contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
using RandomOffset = std::uint64_t;

/**
 * @brief Stores the RandomSequenceVersion1 value for this contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_engines
 */
inline constexpr std::uint32_t kRandomSequenceVersion1 = 1;

/**
 * @brief Stores a serializable SplitMix64 state and sequence version.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
struct SplitMix64State {
  /**
   * @brief Stores the sequence version value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  /**
   * @brief Stores the state value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint64_t state = 0;

  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  friend bool operator==(const SplitMix64State&,
                         const SplitMix64State&) = default;
};

/**
 * @brief Implements the versioned deterministic SplitMix64 engine.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
class ASC_RANDOM_EXPORT SplitMix64 final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  using result_type = std::uint64_t;

  /**
   * @brief Constructs a SplitMix64 with the documented ownership and validity
   * state.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] seed Deterministic seed value.
   * @ingroup asc_random_engines
   */
  explicit SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}

  /**
   * @brief Performs the public min operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  /**
   * @brief Performs the public max operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] result_type operator()() noexcept;
  /**
   * @brief Performs the public ExportState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] SplitMix64State ExportState() const noexcept;
  /**
   * @brief Validates inputs and creates the requested Random engine object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static Result<SplitMix64> FromState(SplitMix64State state);
  /**
   * @brief Performs the public RestoreState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_random_engines
   */
  Status RestoreState(SplitMix64State state);

 private:
  std::uint64_t state_;
};

/**
 * @brief Stores a serializable PCG32 state and sequence version.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
struct Pcg32State {
  /**
   * @brief Stores the sequence version value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  /**
   * @brief Stores the state value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint64_t state = 0;
  /**
   * @brief Stores the increment value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint64_t increment = 1;

  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  friend bool operator==(const Pcg32State&, const Pcg32State&) = default;
};

/**
 * @brief Implements the versioned deterministic PCG32 engine.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
class ASC_RANDOM_EXPORT Pcg32 final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  using result_type = std::uint32_t;

  /**
   * @brief Constructs a Pcg32 with the documented ownership and validity state.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] seed Deterministic seed value.
   * @ingroup asc_random_engines
   */
  explicit Pcg32(std::uint64_t seed) noexcept;
  /**
   * @brief Constructs a Pcg32 with the documented ownership and validity state.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] initial_state The initial state value required by this contract.
   * @param[in] stream CUDA stream whose ordering and lifetime are caller
   * controlled.
   * @ingroup asc_random_engines
   */
  Pcg32(std::uint64_t initial_state, std::uint64_t stream) noexcept;

  /**
   * @brief Performs the public min operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  /**
   * @brief Performs the public max operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] result_type operator()() noexcept;
  /**
   * @brief Performs the public ExportState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] Pcg32State ExportState() const noexcept;
  /**
   * @brief Validates inputs and creates the requested Random engine object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static Result<Pcg32> FromState(Pcg32State state);
  /**
   * @brief Performs the public RestoreState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_random_engines
   */
  Status RestoreState(Pcg32State state);

 private:
  std::uint64_t state_ = 0;
  std::uint64_t increment_ = 1;
};

/**
 * @brief Stores a serializable xoroshiro64* state and sequence version.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
struct Xoroshiro64StarState {
  /**
   * @brief Stores the sequence version value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  /**
   * @brief Stores the first value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint32_t first = 0;
  /**
   * @brief Stores the second value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint32_t second = 0;

  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  friend bool operator==(const Xoroshiro64StarState&,
                         const Xoroshiro64StarState&) = default;
};

/**
 * @brief Implements the versioned deterministic xoroshiro64* engine.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
class ASC_RANDOM_EXPORT Xoroshiro64Star final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  using result_type = std::uint32_t;

  /**
   * @brief Constructs a Xoroshiro64Star with the documented ownership and
   * validity state.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] seed Deterministic seed value.
   * @ingroup asc_random_engines
   */
  explicit Xoroshiro64Star(std::uint64_t seed) noexcept;

  /**
   * @brief Performs the public min operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  /**
   * @brief Performs the public max operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] result_type operator()() noexcept;
  /**
   * @brief Performs the public Jump operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  void Jump() noexcept;
  /**
   * @brief Performs the public LongJump operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  void LongJump() noexcept;
  /**
   * @brief Performs the public ExportState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] Xoroshiro64StarState ExportState() const noexcept;
  /**
   * @brief Validates inputs and creates the requested Random engine object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static Result<Xoroshiro64Star> FromState(
      Xoroshiro64StarState state);
  /**
   * @brief Performs the public RestoreState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_random_engines
   */
  Status RestoreState(Xoroshiro64StarState state);

 private:
  std::array<std::uint32_t, 2> state_;
};

/**
 * @brief Stores a serializable xoroshiro128+ state and sequence version.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
struct Xoroshiro128PlusState {
  /**
   * @brief Stores the sequence version value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint32_t sequence_version = kRandomSequenceVersion1;
  /**
   * @brief Stores the first value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint64_t first = 0;
  /**
   * @brief Stores the second value for this contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  std::uint64_t second = 0;

  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  friend bool operator==(const Xoroshiro128PlusState&,
                         const Xoroshiro128PlusState&) = default;
};

/**
 * @brief Implements the versioned deterministic xoroshiro128+ engine.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_engines
 */
class ASC_RANDOM_EXPORT Xoroshiro128Plus final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  using result_type = std::uint64_t;

  /**
   * @brief Constructs a Xoroshiro128Plus with the documented ownership and
   * validity state.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] seed Deterministic seed value.
   * @ingroup asc_random_engines
   */
  explicit Xoroshiro128Plus(std::uint64_t seed) noexcept;

  /**
   * @brief Performs the public min operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  /**
   * @brief Performs the public max operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] result_type operator()() noexcept;
  /**
   * @brief Performs the public Jump operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  void Jump() noexcept;
  /**
   * @brief Performs the public LongJump operation defined by the Random engine
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_engines
   */
  void LongJump() noexcept;
  /**
   * @brief Performs the public ExportState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] Xoroshiro128PlusState ExportState() const noexcept;
  /**
   * @brief Validates inputs and creates the requested Random engine object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_engines
   */
  [[nodiscard]] static Result<Xoroshiro128Plus> FromState(
      Xoroshiro128PlusState state);
  /**
   * @brief Performs the public RestoreState operation defined by the Random
   * engine contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] state The state value required by this contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_random_engines
   */
  Status RestoreState(Xoroshiro128PlusState state);

 private:
  std::array<std::uint64_t, 2> state_;
};

/**
 * @brief Performs the public Philox4x32_10 operation defined by the Random
 * engine contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] counter The counter value required by this contract.
 * @param[in] key The key value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_random_engines
 */
[[nodiscard]] ASC_RANDOM_EXPORT Philox4x32Result
Philox4x32_10(Philox4x32Counter counter, Philox4x32Key key) noexcept;

/**
 * @brief Generates deterministic Random engine values into the requested
 * destination.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] stream CUDA stream whose ordering and lifetime are caller
 * controlled.
 * @param[in] subsequence Deterministic independent subsequence identifier.
 * @param[in] block The block value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_random_engines
 */
[[nodiscard]] ASC_RANDOM_EXPORT Philox4x32Result
GeneratePhilox4x32Block(RandomStream stream, RandomSubsequence subsequence,
                        std::uint64_t block) noexcept;

/**
 * @brief Generates deterministic Random engine values into the requested
 * destination.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] stream CUDA stream whose ordering and lifetime are caller
 * controlled.
 * @param[in] subsequence Deterministic independent subsequence identifier.
 * @param[in] offset Deterministic address offset within the selected sequence.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_random_engines
 */
[[nodiscard]] ASC_RANDOM_EXPORT std::uint32_t GeneratePhilox4x32Word(
    RandomStream stream, RandomSubsequence subsequence,
    RandomOffset offset) noexcept;

/**
 * @brief Performs the public AdvanceRandomOffset operation defined by the
 * Random engine contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] offset Deterministic address offset within the selected sequence.
 * @param[in] word_count The word count value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_random_engines
 */
[[nodiscard]] ASC_RANDOM_EXPORT Result<RandomOffset> AdvanceRandomOffset(
    RandomOffset offset, std::uint64_t word_count);

}  // namespace asc

#endif  // ASC_RANDOM_ENGINE_H_
