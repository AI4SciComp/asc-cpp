#ifndef ASC_RANDOM_GENERATOR_H_
#define ASC_RANDOM_GENERATOR_H_

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

#include <type_traits>
#include <utility>

#include "asc/core/result.h"
#include "asc/random/distribution.h"

namespace asc {

/**
 * @brief Couples a canonical engine and validated distribution.
 *
 * The engine and distribution are stored by value. Calls advance engine state
 * deterministically and return distribution validation failures through
 * Result. Independent objects are safe to use concurrently; one object is not
 * internally synchronized.
 *
 * @tparam Engine Canonical deterministic engine type.
 * @tparam Distribution Distribution returning `Result<result_type>`.
 * @ingroup asc_random_distributions
 */
template <CanonicalRandomEngine Engine, typename Distribution>
  requires requires(Distribution& distribution, Engine& engine) {
    typename Distribution::result_type;
    {
      distribution(engine)
    } -> std::same_as<Result<typename Distribution::result_type>>;
  }
/**
 * @brief Constrains stateful generators that return Result values.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 * @ingroup asc_random_distributions
 */
class Generator final {
 public:
  /**
   * @brief Defines the public engine_type type used by this Random distribution
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_distributions
   */
  using engine_type = Engine;
  /**
   * @brief Defines the public distribution_type type used by this Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_distributions
   */
  using distribution_type = Distribution;
  /**
   * @brief Defines the public result_type type used by this Random distribution
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @ingroup asc_random_distributions
   */
  using result_type = typename Distribution::result_type;

  /**
   * @brief Constructs a Generator with the documented ownership and validity
   * state.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @param[in] engine The engine value required by this contract.
   * @param[in] distribution The distribution value required by this contract.
   * @ingroup asc_random_distributions
   */
  Generator(Engine engine, Distribution distribution)
      : engine_(std::move(engine)), distribution_(std::move(distribution)) {}

  /**
   * @brief Produces the next deterministic value according to the object's
   * contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Result<result_type> operator()() {
    return distribution_(engine_);
  }

  /**
   * @brief Performs the public engine operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Engine& engine() noexcept { return engine_; }
  /**
   * @brief Performs the public engine operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] const Engine& engine() const noexcept { return engine_; }
  /**
   * @brief Performs the public distribution operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] Distribution& distribution() noexcept { return distribution_; }
  /**
   * @brief Performs the public distribution operation defined by the Random
   * distribution contract.
   *
   * Reproducibility is defined by the documented engine, distribution,
   * seed/subsequence/offset address mapping, and sequence-version boundary.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_random_distributions
   */
  [[nodiscard]] const Distribution& distribution() const noexcept {
    return distribution_;
  }

 private:
  Engine engine_;
  Distribution distribution_;
};

namespace internal_random_generator {

template <typename Value, bool IsInteger>
struct UniformDistributionSelector;

template <SupportedRandomInteger Value>
struct UniformDistributionSelector<Value, true> {
  using type = UniformIntegerDistribution<Value>;
};

template <SupportedRandomReal Value>
struct UniformDistributionSelector<Value, false> {
  using type = UniformRealDistribution<Value>;
};

}  // namespace internal_random_generator

/**
 * @brief Defines the public UniformDistribution type used by this Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @ingroup asc_random_distributions
 */
template <typename Value>
  requires SupportedRandomInteger<Value> || SupportedRandomReal<Value>
using UniformDistribution =
    typename internal_random_generator::UniformDistributionSelector<
        Value, SupportedRandomInteger<Value>>::type;

/**
 * @brief Defines the public UniformGenerator type used by this Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @tparam Engine Type or non-type argument satisfying the declaration's
 * constraints.
 * @ingroup asc_random_distributions
 */
template <CanonicalRandomEngine Engine, typename Value>
  requires SupportedRandomInteger<Value> || SupportedRandomReal<Value>
using UniformGenerator = Generator<Engine, UniformDistribution<Value>>;

/**
 * @brief Defines the public NormalGenerator type used by this Random
 * distribution contract.
 *
 * Reproducibility is defined by the documented engine, distribution,
 * seed/subsequence/offset address mapping, and sequence-version boundary.
 *
 * @tparam Engine Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Real Type or non-type argument satisfying the declaration's
 * constraints.
 * @ingroup asc_random_distributions
 */
template <CanonicalRandomEngine Engine, SupportedRandomReal Real>
using NormalGenerator = Generator<Engine, NormalDistribution<Real>>;

}  // namespace asc

#endif  // ASC_RANDOM_GENERATOR_H_
