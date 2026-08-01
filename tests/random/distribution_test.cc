#include "asc/random/distribution.h"

#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "test_support.h"

namespace {

template <typename Word, std::size_t Size>
class ScriptedEngine final {
 public:
  using result_type = Word;

  explicit ScriptedEngine(std::array<Word, Size> words) : words_(words) {}

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }

  [[nodiscard]] result_type operator()() noexcept {
    const std::size_t position = calls_ < Size ? calls_ : Size - 1;
    ++calls_;
    return words_[position];
  }

  [[nodiscard]] std::size_t calls() const noexcept { return calls_; }

 private:
  std::array<Word, Size> words_;
  std::size_t calls_ = 0;
};

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

constexpr float kFloatPi = 0x1.921fb6p+1F;
constexpr double kDoublePi = 0x1.921fb54442d18p+1;
static_assert(std::bit_cast<std::uint32_t>(kFloatPi) == 0x40490FDBU);
static_assert(std::bit_cast<std::uint64_t>(kDoublePi) == 0x400921FB54442D18ULL);

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

void CheckUniformInteger(asc_random_test::TestContext& context) {
  auto byte_distribution =
      asc::UniformIntegerDistribution<std::uint8_t>::Create(0, 9);
  ASC_RANDOM_TEST_CHECK(context, byte_distribution.ok());
  ScriptedEngine<std::uint32_t, 2> rejection_engine({0x05000000U, 0x06000000U});
  const auto rejected_then_accepted = (*byte_distribution)(rejection_engine);
  ASC_RANDOM_TEST_CHECK(context, rejected_then_accepted.ok());
  ASC_RANDOM_TEST_EQ(context, *rejected_then_accepted, 6U);
  ASC_RANDOM_TEST_EQ(context, rejection_engine.calls(), 2U);

  auto full_signed = asc::UniformIntegerDistribution<std::int32_t>::Create(
      std::numeric_limits<std::int32_t>::lowest(),
      std::numeric_limits<std::int32_t>::max());
  ASC_RANDOM_TEST_CHECK(context, full_signed.ok());
  ScriptedEngine<std::uint32_t, 2> full_engine(
      {0U, std::numeric_limits<std::uint32_t>::max()});
  ASC_RANDOM_TEST_EQ(context, *(*full_signed)(full_engine),
                     std::numeric_limits<std::int32_t>::lowest());
  ASC_RANDOM_TEST_EQ(context, *(*full_signed)(full_engine),
                     std::numeric_limits<std::int32_t>::max());

  auto signed_interval =
      asc::UniformIntegerDistribution<std::int64_t>::Create(-5, 5);
  ASC_RANDOM_TEST_CHECK(context, signed_interval.ok());
  ScriptedEngine<std::uint64_t, 1> signed_engine({6});
  const auto signed_value = (*signed_interval)(signed_engine);
  ASC_RANDOM_TEST_CHECK(context, signed_value.ok());
  ASC_RANDOM_TEST_CHECK(context, *signed_value >= -5 && *signed_value <= 5);

  ASC_RANDOM_TEST_EQ(
      context,
      asc::UniformIntegerDistribution<std::uint64_t>::Create(9, 8)
          .status()
          .code(),
      asc::ErrorCode::kInvalidArgument);
}

void CheckUniformReal(asc_random_test::TestContext& context) {
  auto distribution = asc::UniformRealDistribution<double>::Create(2, 6);
  ASC_RANDOM_TEST_CHECK(context, distribution.ok());
  ScriptedEngine<std::uint64_t, 1> midpoint_engine({0x8000000000000000ULL});
  ASC_RANDOM_TEST_EQ(context, *(*distribution)(midpoint_engine), 4.0);
  ASC_RANDOM_TEST_EQ(context, midpoint_engine.calls(), 1U);

  ScriptedEngine<std::uint32_t, 2> double_engine({0x80000000U, 0U});
  ASC_RANDOM_TEST_EQ(context, *(*distribution)(double_engine), 4.0);
  ASC_RANDOM_TEST_EQ(context, double_engine.calls(), 2U);

  auto float_distribution =
      asc::UniformRealDistribution<float>::Create(2.0F, 6.0F);
  ASC_RANDOM_TEST_CHECK(context, float_distribution.ok());
  ScriptedEngine<std::uint32_t, 1> float_midpoint_engine({0x80000000U});
  ASC_RANDOM_TEST_EQ(context, *(*float_distribution)(float_midpoint_engine),
                     4.0F);
  ASC_RANDOM_TEST_EQ(context, float_midpoint_engine.calls(), 1U);

  const double lower = 1.0;
  const double upper = std::nextafter(lower, 2.0);
  auto narrow = asc::UniformRealDistribution<double>::Create(lower, upper);
  ASC_RANDOM_TEST_CHECK(context, narrow.ok());
  ScriptedEngine<std::uint64_t, 1> endpoint_engine(
      {std::numeric_limits<std::uint64_t>::max()});
  const auto repaired = (*narrow)(endpoint_engine);
  ASC_RANDOM_TEST_CHECK(context, repaired.ok());
  ASC_RANDOM_TEST_EQ(context, *repaired, lower);

  const float float_lower = 1.0F;
  const float float_upper = std::nextafter(float_lower, 2.0F);
  auto float_narrow =
      asc::UniformRealDistribution<float>::Create(float_lower, float_upper);
  ASC_RANDOM_TEST_CHECK(context, float_narrow.ok());
  ScriptedEngine<std::uint32_t, 1> float_endpoint_engine(
      {std::numeric_limits<std::uint32_t>::max()});
  ASC_RANDOM_TEST_EQ(context, *(*float_narrow)(float_endpoint_engine),
                     float_lower);

  ASC_RANDOM_TEST_EQ(
      context,
      asc::UniformRealDistribution<double>::Create(1, 1).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context,
                     asc::UniformRealDistribution<double>::Create(
                         -std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::max())
                         .status()
                         .code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context,
                     asc::UniformRealDistribution<float>::Create(
                         0, std::numeric_limits<float>::infinity())
                         .status()
                         .code(),
                     asc::ErrorCode::kInvalidArgument);
}

void CheckNormal(asc_random_test::TestContext& context) {
  auto distribution = asc::NormalDistribution<double>::Create(0, 1);
  ASC_RANDOM_TEST_CHECK(context, distribution.ok());
  ScriptedEngine<std::uint64_t, 2> engine({0x8000000000000000ULL, 0U});
  const auto value = (*distribution)(engine);
  ASC_RANDOM_TEST_CHECK(context, value.ok());
  const double first_unit = 0.5;
  const double second_unit = 0.0;
  const double unit_radius = 1.0 - first_unit;
  const double unit_angle = second_unit;
  const double radius = std::sqrt(-2.0 * std::log(unit_radius));
  const double z = radius * std::cos(2.0 * kDoublePi * unit_angle);
  const double expected = std::fma(1.0, z, 0.0);
  ASC_RANDOM_TEST_EQ(context, *value, expected);
  ASC_RANDOM_TEST_CHECK(context,
                        std::abs(expected - 1.1774100225154747) < 1.0e-15);
  ASC_RANDOM_TEST_EQ(context, engine.calls(), 2U);

  auto float_distribution = asc::NormalDistribution<float>::Create(0, 1);
  ASC_RANDOM_TEST_CHECK(context, float_distribution.ok());
  ScriptedEngine<std::uint32_t, 2> float_engine({0x80000000U, 0U});
  const float float_unit_radius = 1.0F - 0.5F;
  const float float_radius = std::sqrt(-2.0F * std::log(float_unit_radius));
  const float float_z = float_radius * std::cos(2.0F * kFloatPi * 0.0F);
  const float float_expected = std::fma(1.0F, float_z, 0.0F);
  ASC_RANDOM_TEST_EQ(context, *(*float_distribution)(float_engine),
                     float_expected);
  ASC_RANDOM_TEST_EQ(context, float_engine.calls(), 2U);

  ScriptedEngine<std::uint32_t, 4> zero_engine({0U, 0U, 0U, 0U});
  auto shifted = asc::NormalDistribution<double>::Create(3, 2);
  ASC_RANDOM_TEST_CHECK(context, shifted.ok());
  ASC_RANDOM_TEST_EQ(context, *(*shifted)(zero_engine), 3.0);
  ASC_RANDOM_TEST_EQ(context, zero_engine.calls(), 4U);

  ASC_RANDOM_TEST_EQ(
      context, asc::NormalDistribution<double>::Create(0, 0).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context,
                     asc::NormalDistribution<double>::Create(
                         std::numeric_limits<double>::infinity(), 1)
                         .status()
                         .code(),
                     asc::ErrorCode::kInvalidArgument);

  auto overflowing = asc::NormalDistribution<double>::Create(
      std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
  ASC_RANDOM_TEST_CHECK(context, overflowing.ok());
  ScriptedEngine<std::uint64_t, 2> overflow_engine({0x8000000000000000ULL, 0U});
  const auto failed = (*overflowing)(overflow_engine);
  ASC_RANDOM_TEST_EQ(context, failed.status().code(),
                     asc::ErrorCode::kNumerical);
  ASC_RANDOM_TEST_EQ(context, overflow_engine.calls(), 2U);
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckFloatTransform(context);
  CheckDoubleTransform(context);
  CheckUniformInteger(context);
  CheckUniformReal(context);
  CheckNormal(context);
  return context.Finish();
}
