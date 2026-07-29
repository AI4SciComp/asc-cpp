#include "asc/random/distribution.h"

#include <bit>
#include <concepts>
#include <cstdint>

#include "test_support.h"

namespace {

template <typename Real>
concept HasOneWordUniform = requires(std::uint32_t word) {
  { asc::Uniform01<Real>(word) } -> std::same_as<Real>;
};

template <typename Real>
concept HasTwoWordUniform =
    requires(std::uint32_t high_word, std::uint32_t low_word) {
      { asc::Uniform01<Real>(high_word, low_word) } -> std::same_as<Real>;
    };

static_assert(HasOneWordUniform<float>);
static_assert(!HasOneWordUniform<double>);
static_assert(!HasOneWordUniform<long double>);
static_assert(HasTwoWordUniform<double>);
static_assert(!HasTwoWordUniform<float>);
static_assert(!HasTwoWordUniform<long double>);
static_assert(noexcept(asc::Uniform01<float>(0)));
static_assert(noexcept(asc::Uniform01<double>(0, 0)));

void CheckFloatTransform(asc_random_test::TestContext& context) {
  struct Vector {
    std::uint32_t raw;
    std::uint32_t expected_bits;
  };
  constexpr Vector kVectors[] = {
      {0x00000000U, 0x00000000U}, {0xFFFFFFFFU, 0x3F7FFFFFU},
      {0x80000000U, 0x3F000000U}, {0x12345678U, 0x3D91A2B0U},
      {0x000001FFU, 0x33800000U},
  };
  for (const Vector& vector : kVectors) {
    const float value = asc::Uniform01<float>(vector.raw);
    ASC_RANDOM_TEST_EQ(context, std::bit_cast<std::uint32_t>(value),
                       vector.expected_bits);
    ASC_RANDOM_TEST_CHECK(context, value >= 0.0F);
    ASC_RANDOM_TEST_CHECK(context, value < 1.0F);
  }

  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<float>(0x12345600U),
                     asc::Uniform01<float>(0x123456FFU));
  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<float>(0x00000000U), 0.0F);
}

void CheckDoubleTransform(asc_random_test::TestContext& context) {
  struct Vector {
    std::uint32_t high_word;
    std::uint32_t low_word;
    std::uint64_t expected_bits;
  };
  constexpr Vector kVectors[] = {
      {0x00000000U, 0x00000000U, 0x0000000000000000ULL},
      {0xFFFFFFFFU, 0xFFFFFFFFU, 0x3FEFFFFFFFFFFFFFULL},
      {0x80000000U, 0x00000000U, 0x3FE0000000000000ULL},
      {0x12345678U, 0x9ABCDEF0U, 0x3FB23456789ABCD8ULL},
      {0x00000000U, 0xFFFFFFFFU, 0x3DEFFFFF00000000ULL},
  };
  for (const Vector& vector : kVectors) {
    const double value =
        asc::Uniform01<double>(vector.high_word, vector.low_word);
    ASC_RANDOM_TEST_EQ(context, std::bit_cast<std::uint64_t>(value),
                       vector.expected_bits);
    ASC_RANDOM_TEST_CHECK(context, value >= 0.0);
    ASC_RANDOM_TEST_CHECK(context, value < 1.0);
  }

  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<double>(0x12345678U, 0x9ABCDE00U),
                     asc::Uniform01<double>(0x12345678U, 0x9ABCDEFFU));
  ASC_RANDOM_TEST_EQ(context, asc::Uniform01<double>(0U, 0x000007FFU), 0.0);
  ASC_RANDOM_TEST_CHECK(context,
                        asc::Uniform01<double>(0x12345678U, 0x9ABCDEF0U) !=
                            asc::Uniform01<double>(0x9ABCDEF0U, 0x12345678U));
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckFloatTransform(context);
  CheckDoubleTransform(context);
  return context.Finish();
}
