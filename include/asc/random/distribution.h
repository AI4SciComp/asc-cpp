#ifndef ASC_RANDOM_DISTRIBUTION_H_
#define ASC_RANDOM_DISTRIBUTION_H_

/**
 * @file
 * @brief Public Random distribution declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_random_distributions
 */

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <numbers>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/export.h"

namespace asc {

/**
 * @brief Performs the public Uniform01 operation defined by the Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] word The word value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_random_distributions
 */
template <typename Real>
Real Uniform01(std::uint32_t word) noexcept = delete;

/**
 * @brief Performs the public Uniform01 operation defined by the Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] word The word value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_random_distributions
 */
template <>
[[nodiscard]] ASC_RANDOM_EXPORT float Uniform01<float>(
    std::uint32_t word) noexcept;

/**
 * @brief Performs the public Uniform01 operation defined by the Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] high_word The high word value required by this contract.
 * @param[in] low_word The low word value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_random_distributions
 */
template <typename Real>
Real Uniform01(std::uint32_t high_word,
               std::uint32_t low_word) noexcept = delete;

/**
 * @brief Performs the public Uniform01 operation defined by the Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @param[in] high_word The high word value required by this contract.
 * @param[in] low_word The low word value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_random_distributions
 */
template <>
[[nodiscard]] ASC_RANDOM_EXPORT double Uniform01<double>(
    std::uint32_t high_word, std::uint32_t low_word) noexcept;

/**
 * @brief Defines the public SupportedRandomInteger concept contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
template <typename Integer>
concept SupportedRandomInteger =
    std::integral<Integer> && !std::same_as<std::remove_cv_t<Integer>, bool> &&
    (sizeof(Integer) == 1 || sizeof(Integer) == 2 || sizeof(Integer) == 4 ||
     sizeof(Integer) == 8) &&
    (std::numeric_limits<std::make_unsigned_t<Integer>>::digits ==
     sizeof(Integer) * 8);

/**
 * @brief Defines the public SupportedRandomReal concept contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
template <typename Real>
concept SupportedRandomReal =
    std::same_as<Real, float> || std::same_as<Real, double>;

/**
 * @brief Defines the public CanonicalRandomEngine concept contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
template <typename Engine>
concept CanonicalRandomEngine =
    requires(Engine& engine) {
      typename Engine::result_type;
      { engine() } noexcept -> std::same_as<typename Engine::result_type>;
      { Engine::min() } -> std::same_as<typename Engine::result_type>;
      { Engine::max() } -> std::same_as<typename Engine::result_type>;
    } &&
    (std::same_as<typename Engine::result_type, std::uint32_t> ||
     std::same_as<typename Engine::result_type, std::uint64_t>) &&
    (Engine::min() == 0) &&
    (Engine::max() == std::numeric_limits<typename Engine::result_type>::max());

namespace internal_random_distribution {

template <int Bits, CanonicalRandomEngine Engine>
[[nodiscard]] std::uint64_t CanonicalBits(Engine& engine) noexcept {
  static_assert(Bits > 0 && Bits <= 64);
  constexpr int kEngineBits =
      std::numeric_limits<typename Engine::result_type>::digits;
  if constexpr (kEngineBits == 64) {
    return static_cast<std::uint64_t>(engine()) >> (64 - Bits);
  } else if constexpr (Bits <= 32) {
    return static_cast<std::uint64_t>(engine() >> (32 - Bits));
  } else {
    const std::uint64_t high = engine();
    const std::uint64_t low = engine();
    return ((high << 32U) | low) >> (64 - Bits);
  }
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
[[nodiscard]] Real UnitReal(Engine& engine) noexcept {
  if constexpr (std::same_as<Real, float>) {
    constexpr float kScale = 0x1.0p-24F;
    return static_cast<float>(CanonicalBits<24>(engine)) * kScale;
  } else {
    constexpr double kScale = 0x1.0p-53;
    return static_cast<double>(CanonicalBits<53>(engine)) * kScale;
  }
}

template <SupportedRandomInteger Integer>
[[nodiscard]] Integer AddOffset(Integer lower,
                                std::make_unsigned_t<Integer> offset) noexcept {
  using Unsigned = std::make_unsigned_t<Integer>;
  if constexpr (std::is_unsigned_v<Integer>) {
    return static_cast<Integer>(static_cast<Unsigned>(lower) + offset);
  } else {
    if (lower >= 0) {
      return static_cast<Integer>(static_cast<Unsigned>(lower) + offset);
    }
    const Unsigned negative_count = Unsigned{0} - static_cast<Unsigned>(lower);
    if (offset < negative_count) {
      const Unsigned magnitude = negative_count - offset;
      const Unsigned minimum_magnitude =
          static_cast<Unsigned>(std::numeric_limits<Integer>::max()) + 1U;
      if (magnitude == minimum_magnitude) {
        return std::numeric_limits<Integer>::lowest();
      }
      return static_cast<Integer>(-static_cast<Integer>(magnitude));
    }
    return static_cast<Integer>(offset - negative_count);
  }
}

}  // namespace internal_random_distribution

/**
 * @brief Maps engine output uniformly to a validated integer interval.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
template <SupportedRandomInteger Integer>
class UniformIntegerDistribution final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random distribution
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_distributions
   */
  using result_type = Integer;

  /**
   * @brief Validates inputs and creates the requested Random distribution
   * object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] lower Inclusive lower distribution bound.
   * @param[in] upper Exclusive upper distribution bound unless documented
   * otherwise.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] static Result<UniformIntegerDistribution> Create(
      Integer lower, Integer upper) {
    if (lower > upper) {
      return Status(ErrorCode::kInvalidArgument,
                    "Uniform integer bounds are reversed");
    }
    return UniformIntegerDistribution(lower, upper);
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @tparam Engine Type or non-type argument satisfying the declaration's
   * constraints.
   * @param[in] engine The engine value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  template <CanonicalRandomEngine Engine>
  [[nodiscard]] Result<Integer> operator()(Engine& engine) const {
    using Unsigned = std::make_unsigned_t<Integer>;
    constexpr int kBits = std::numeric_limits<Unsigned>::digits;
    const Unsigned range =
        static_cast<Unsigned>(static_cast<Unsigned>(upper_) -
                              static_cast<Unsigned>(lower_) + Unsigned{1});
    if (range == 0) {
      const Unsigned candidate = static_cast<Unsigned>(
          internal_random_distribution::CanonicalBits<kBits>(engine));
      return internal_random_distribution::AddOffset(lower_, candidate);
    }

    const Unsigned wrapped_negative =
        std::numeric_limits<Unsigned>::max() - range + Unsigned{1};
    const Unsigned threshold = wrapped_negative % range;
    while (true) {
      const Unsigned candidate = static_cast<Unsigned>(
          internal_random_distribution::CanonicalBits<kBits>(engine));
      if (candidate >= threshold) {
        return internal_random_distribution::AddOffset(lower_,
                                                       candidate % range);
      }
    }
  }

  /**
   * @brief Performs the public lower operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Integer lower() const noexcept { return lower_; }
  /**
   * @brief Performs the public upper operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Integer upper() const noexcept { return upper_; }

 private:
  UniformIntegerDistribution(Integer lower, Integer upper) noexcept
      : lower_(lower), upper_(upper) {}

  Integer lower_;
  Integer upper_;
};

/**
 * @brief Maps engine output uniformly to a validated half-open real interval.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
template <SupportedRandomReal Real>
class UniformRealDistribution final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random distribution
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_distributions
   */
  using result_type = Real;

  /**
   * @brief Validates inputs and creates the requested Random distribution
   * object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] lower Inclusive lower distribution bound.
   * @param[in] upper Exclusive upper distribution bound unless documented
   * otherwise.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] static Result<UniformRealDistribution> Create(Real lower,
                                                              Real upper) {
    if (!std::isfinite(lower) || !std::isfinite(upper) || !(lower < upper)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Uniform real bounds must be finite and ordered");
    }
    const Real span = upper - lower;
    if (!std::isfinite(span)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Uniform real span must be finite");
    }
    return UniformRealDistribution(lower, upper, span);
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @tparam Engine Type or non-type argument satisfying the declaration's
   * constraints.
   * @param[in] engine The engine value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  template <CanonicalRandomEngine Engine>
  [[nodiscard]] Result<Real> operator()(Engine& engine) const {
    const Real unit = internal_random_distribution::UnitReal<Real>(engine);
    Real candidate = std::fma(span_, unit, lower_);
    if (candidate == upper_) {
      candidate = std::nextafter(upper_, lower_);
    }
    if (!std::isfinite(candidate) || candidate < lower_ ||
        !(candidate < upper_)) {
      return Status(ErrorCode::kNumerical,
                    "Uniform real transform produced an invalid result");
    }
    return candidate;
  }

  /**
   * @brief Performs the public lower operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Real lower() const noexcept { return lower_; }
  /**
   * @brief Performs the public upper operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Real upper() const noexcept { return upper_; }

 private:
  UniformRealDistribution(Real lower, Real upper, Real span) noexcept
      : lower_(lower), upper_(upper), span_(span) {}

  Real lower_;
  Real upper_;
  Real span_;
};

/**
 * @brief Transforms engine words into normal real samples.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
template <SupportedRandomReal Real>
class NormalDistribution final {
 public:
  /**
   * @brief Defines the public result_type type used by this Random distribution
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_distributions
   */
  using result_type = Real;

  /**
   * @brief Validates inputs and creates the requested Random distribution
   * object.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] mean The mean value required by this contract.
   * @param[in] standard_deviation The standard deviation value required by this
   * contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] static Result<NormalDistribution> Create(
      Real mean, Real standard_deviation) {
    if (!std::isfinite(mean) || !std::isfinite(standard_deviation) ||
        !(standard_deviation > Real{0})) {
      return Status(
          ErrorCode::kInvalidArgument,
          "Normal parameters require a finite mean and positive deviation");
    }
    return NormalDistribution(mean, standard_deviation);
  }

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @tparam Engine Type or non-type argument satisfying the declaration's
   * constraints.
   * @param[in] engine The engine value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  template <CanonicalRandomEngine Engine>
  [[nodiscard]] Result<Real> operator()(Engine& engine) const {
    const Real first_unit =
        internal_random_distribution::UnitReal<Real>(engine);
    const Real second_unit =
        internal_random_distribution::UnitReal<Real>(engine);
    const Real unit_radius = Real{1} - first_unit;
    const Real unit_angle = second_unit;
    constexpr Real kPi = std::numbers::pi_v<Real>;
    const Real radius = std::sqrt(Real{-2} * std::log(unit_radius));
    const Real z = radius * std::cos(Real{2} * kPi * unit_angle);
    const Real candidate = std::fma(standard_deviation_, z, mean_);
    if (!std::isfinite(radius) || !std::isfinite(z) ||
        !std::isfinite(candidate)) {
      return Status(ErrorCode::kNumerical,
                    "Normal transform produced a nonfinite result");
    }
    return candidate;
  }

  /**
   * @brief Performs the public mean operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Real mean() const noexcept { return mean_; }
  /**
   * @brief Performs the public standard_deviation operation defined by the
   * Random distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Real standard_deviation() const noexcept {
    return standard_deviation_;
  }

 private:
  NormalDistribution(Real mean, Real standard_deviation) noexcept
      : mean_(mean), standard_deviation_(standard_deviation) {}

  Real mean_;
  Real standard_deviation_;
};

}  // namespace asc

#endif  // ASC_RANDOM_DISTRIBUTION_H_
