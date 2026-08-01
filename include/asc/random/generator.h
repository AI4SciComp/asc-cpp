#ifndef ASC_RANDOM_GENERATOR_H_
#define ASC_RANDOM_GENERATOR_H_

#include <type_traits>
#include <utility>

#include "asc/core/result.h"
#include "asc/random/distribution.h"

namespace asc {

template <CanonicalRandomEngine Engine, typename Distribution>
  requires requires(Distribution& distribution, Engine& engine) {
    typename Distribution::result_type;
    {
      distribution(engine)
    } -> std::same_as<Result<typename Distribution::result_type>>;
  }
class Generator final {
 public:
  using engine_type = Engine;
  using distribution_type = Distribution;
  using result_type = typename Distribution::result_type;

  Generator(Engine engine, Distribution distribution)
      : engine_(std::move(engine)), distribution_(std::move(distribution)) {}

  [[nodiscard]] Result<result_type> operator()() {
    return distribution_(engine_);
  }

  [[nodiscard]] Engine& engine() noexcept { return engine_; }
  [[nodiscard]] const Engine& engine() const noexcept { return engine_; }
  [[nodiscard]] Distribution& distribution() noexcept { return distribution_; }
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

template <typename Value>
  requires SupportedRandomInteger<Value> || SupportedRandomReal<Value>
using UniformDistribution =
    typename internal_random_generator::UniformDistributionSelector<
        Value, SupportedRandomInteger<Value>>::type;

template <CanonicalRandomEngine Engine, typename Value>
  requires SupportedRandomInteger<Value> || SupportedRandomReal<Value>
using UniformGenerator = Generator<Engine, UniformDistribution<Value>>;

template <CanonicalRandomEngine Engine, SupportedRandomReal Real>
using NormalGenerator = Generator<Engine, NormalDistribution<Real>>;

}  // namespace asc

#endif  // ASC_RANDOM_GENERATOR_H_
