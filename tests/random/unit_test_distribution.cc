#include <asc/random/distribution.h>

#include <gtest/gtest.h>

#include <concepts>
#include <cmath>
#include <cstdint>
#include <limits>

namespace asc {
namespace {

template <typename T>
concept HasUniform01 = requires { typename Uniform01<T>; };

static_assert(HasUniform01<float>);
static_assert(HasUniform01<double>);
static_assert(!HasUniform01<bool>);
static_assert(!HasUniform01<int>);
static_assert(!HasUniform01<unsigned int>);
static_assert(!HasUniform01<long double>);
static_assert(!HasUniform01<volatile float>);
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(std::numeric_limits<double>::is_iec559);

TEST(Uniform01Test, FloatEndpointsAndMidpointAreExact) {
  constexpr Uniform01<float> distribution;
  const float zero = distribution(0x0000000000000000ULL);
  const float half = distribution(0x8000000000000000ULL);
  const float maximum = distribution(0xffffffffffffffffULL);

  EXPECT_EQ(zero, 0.0F);
  EXPECT_FALSE(std::signbit(zero));
  EXPECT_EQ(half, 0.5F);
  EXPECT_EQ(maximum, 1.0F - 0x1.0p-24F);
  EXPECT_LT(maximum, 1.0F);
}

TEST(Uniform01Test, DoubleEndpointsAndMidpointAreExact) {
  constexpr Uniform01<double> distribution;
  const double zero = distribution(0x0000000000000000ULL);
  const double half = distribution(0x8000000000000000ULL);
  const double maximum = distribution(0xffffffffffffffffULL);

  EXPECT_EQ(zero, 0.0);
  EXPECT_FALSE(std::signbit(zero));
  EXPECT_EQ(half, 0.5);
  EXPECT_EQ(maximum, 1.0 - 0x1.0p-53);
  EXPECT_LT(maximum, 1.0);
}

TEST(Uniform01Test, IgnoresBitsBelowDocumentedPrecision) {
  constexpr Uniform01<float> float_distribution;
  constexpr Uniform01<double> double_distribution;

  EXPECT_EQ(float_distribution(0x1234560000000000ULL),
            float_distribution(0x123456ffffffffffULL));
  EXPECT_EQ(double_distribution(0x123456789abc3800ULL),
            double_distribution(0x123456789abc3fffULL));
}

TEST(Uniform01Test, MatchesIndependentKnownWordValues) {
  constexpr Uniform01<float> float_distribution;
  constexpr Uniform01<double> double_distribution;
  constexpr std::uint64_t kWord = 0x6627e8d5e169c58dULL;

  EXPECT_EQ(float_distribution(kWord), 0x1.989fa0p-2F);
  EXPECT_EQ(double_distribution(kWord), 0x1.989fa35785a70p-2);
}

}  // namespace
}  // namespace asc
