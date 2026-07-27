#include "asc/random/distribution.h"

#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>

#include "test_support.h"

namespace {

template <typename Real>
concept HasSingleWordUniform = requires(std::uint32_t word) {
  { asc::Uniform01<Real>(word) } -> std::same_as<Real>;
};

template <typename Real>
concept HasDoubleWordUniform =
    requires(std::uint32_t high_word, std::uint32_t low_word) {
      { asc::Uniform01<Real>(high_word, low_word) } -> std::same_as<Real>;
    };

static_assert(HasSingleWordUniform<float>);
static_assert(!HasSingleWordUniform<double>);
static_assert(!HasDoubleWordUniform<float>);
static_assert(HasDoubleWordUniform<double>);

float ReferenceFloat(std::uint32_t word) {
  return static_cast<float>(word >> 8U) * 0x1p-24F;
}

double ReferenceDouble(std::uint32_t high_word, std::uint32_t low_word) {
  const std::uint64_t joined =
      (static_cast<std::uint64_t>(high_word) << 32U) | low_word;
  return static_cast<double>(joined >> 11U) * 0x1p-53;
}

void CheckFloatTransform(asc_random_test::TestContext& context) {
  constexpr std::uint32_t kWords[] = {
      0x00000000U, 0x00000001U, 0x000000ffU, 0x00000100U,
      0x80000000U, 0x12345678U, 0xfffffeffU, 0xffffffffU,
  };

  for (std::uint32_t word : kWords) {
    const float actual = asc::Uniform01<float>(word);
    const float expected = ReferenceFloat(word);
    ASC_RANDOM_TEST_EQ(context, std::bit_cast<std::uint32_t>(actual),
                       std::bit_cast<std::uint32_t>(expected));
    ASC_RANDOM_TEST_CHECK(context, actual >= 0.0F);
    ASC_RANDOM_TEST_CHECK(context, actual < 1.0F);
  }

  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<float>(0),
                     std::numeric_limits<float>::min() * 0.0F);
  ASC_RANDOM_TEST_EQ(context, std::signbit(asc::Uniform01<float>(0)), false);
  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<float>(0x12345600U),
                     asc::Uniform01<float>(0x123456ffU));
  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<float>(0xffffffffU),
                     1.0F - 0x1p-24F);
}

void CheckDoubleTransform(asc_random_test::TestContext& context) {
  struct Words {
    std::uint32_t high;
    std::uint32_t low;
  };
  constexpr Words kWords[] = {
      {0x00000000U, 0x00000000U}, {0x00000000U, 0x00000001U},
      {0x00000000U, 0x000007ffU}, {0x00000000U, 0x00000800U},
      {0x80000000U, 0x00000000U}, {0x01234567U, 0x89abcdefU},
      {0xffffffffU, 0xfffff800U}, {0xffffffffU, 0xffffffffU},
  };

  for (Words words : kWords) {
    const double actual = asc::Uniform01<double>(words.high, words.low);
    const double expected = ReferenceDouble(words.high, words.low);
    ASC_RANDOM_TEST_EQ(context, std::bit_cast<std::uint64_t>(actual),
                       std::bit_cast<std::uint64_t>(expected));
    ASC_RANDOM_TEST_CHECK(context, actual >= 0.0);
    ASC_RANDOM_TEST_CHECK(context, actual < 1.0);
  }

  ASC_RANDOM_TEST_EQ(context, std::signbit(asc::Uniform01<double>(0, 0)),
                     false);
  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<double>(0x01234567U, 0x89abc800U),
                     asc::Uniform01<double>(0x01234567U, 0x89abcfffU));
  ASC_RANDOM_TEST_CHECK(context,
                        asc::Uniform01<double>(0x01234567U, 0x89abcdefU) !=
                            asc::Uniform01<double>(0x89abcdefU, 0x01234567U));
  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<double>(0xffffffffU, 0xffffffffU),
                     1.0 - 0x1p-53);
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckFloatTransform(context);
  CheckDoubleTransform(context);
  return context.Finish();
}
