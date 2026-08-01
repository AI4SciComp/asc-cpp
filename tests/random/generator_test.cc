#include "asc/random/generator.h"

#include <cstdint>
#include <type_traits>
#include <utility>

#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "test_support.h"

namespace {

using IntegerGenerator = asc::UniformGenerator<asc::Pcg32, std::int32_t>;
using RealGenerator = asc::UniformGenerator<asc::SplitMix64, double>;
using GaussianGenerator = asc::NormalGenerator<asc::Xoroshiro128Plus, double>;
static_assert(std::is_same_v<
              IntegerGenerator,
              asc::Generator<asc::Pcg32,
                             asc::UniformIntegerDistribution<std::int32_t>>>);
static_assert(
    std::is_same_v<
        RealGenerator,
        asc::Generator<asc::SplitMix64, asc::UniformRealDistribution<double>>>);
static_assert(std::is_same_v<GaussianGenerator,
                             asc::Generator<asc::Xoroshiro128Plus,
                                            asc::NormalDistribution<double>>>);
static_assert(std::is_copy_constructible_v<IntegerGenerator>);
static_assert(std::is_move_constructible_v<IntegerGenerator>);
static_assert(!std::is_polymorphic_v<IntegerGenerator>);

void CheckComposition(asc_random_test::TestContext& context) {
  auto integer_distribution =
      asc::UniformIntegerDistribution<std::int32_t>::Create(-17, 31);
  ASC_RANDOM_TEST_CHECK(context, integer_distribution.ok());
  IntegerGenerator integer_generator(asc::Pcg32(42, 54), *integer_distribution);
  IntegerGenerator copied = integer_generator;
  for (int index = 0; index < 32; ++index) {
    const auto first = integer_generator();
    const auto second = copied();
    ASC_RANDOM_TEST_CHECK(context, first.ok());
    ASC_RANDOM_TEST_CHECK(context, second.ok());
    ASC_RANDOM_TEST_EQ(context, *first, *second);
    ASC_RANDOM_TEST_CHECK(context, *first >= -17 && *first <= 31);
  }

  IntegerGenerator moved = std::move(copied);
  IntegerGenerator copied_after_move = copied;
  ASC_RANDOM_TEST_EQ(context, *moved(), *copied_after_move());
  ASC_RANDOM_TEST_EQ(context, integer_generator.distribution().lower(), -17);
  ASC_RANDOM_TEST_EQ(context, integer_generator.distribution().upper(), 31);
}

void CheckEngineOwnership(asc_random_test::TestContext& context) {
  auto real_distribution = asc::UniformRealDistribution<double>::Create(2, 3);
  ASC_RANDOM_TEST_CHECK(context, real_distribution.ok());
  RealGenerator generator(asc::SplitMix64(99), *real_distribution);
  const asc::SplitMix64State before = generator.engine().ExportState();
  const auto value = generator();
  ASC_RANDOM_TEST_CHECK(context, value.ok());
  ASC_RANDOM_TEST_CHECK(context, *value >= 2 && *value < 3);
  ASC_RANDOM_TEST_CHECK(context, generator.engine().ExportState() != before);

  auto normal_distribution = asc::NormalDistribution<double>::Create(0, 1);
  ASC_RANDOM_TEST_CHECK(context, normal_distribution.ok());
  GaussianGenerator normal(asc::Xoroshiro128Plus(123), *normal_distribution);
  GaussianGenerator normal_copy = normal;
  ASC_RANDOM_TEST_EQ(context, *normal(), *normal_copy());
}

void CheckInvalidParameters(asc_random_test::TestContext& context) {
  asc::Pcg32 engine(42, 54);
  const asc::Pcg32State before = engine.ExportState();
  ASC_RANDOM_TEST_EQ(
      context,
      asc::UniformIntegerDistribution<std::int32_t>::Create(7, -3)
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context, engine.ExportState(), before);
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckComposition(context);
  CheckEngineOwnership(context);
  CheckInvalidParameters(context);
  return context.Finish();
}
