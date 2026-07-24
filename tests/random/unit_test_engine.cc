#include <asc/random/counter_engine.h>
#include <asc/random/types.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <type_traits>

namespace asc {
namespace {

template <typename Engine>
concept HasSeed = requires(Engine& engine) { engine.Seed(0); };

template <typename Engine>
concept HasReset = requires(Engine& engine) { engine.Reset(); };

template <typename Engine>
concept HasSkip = requires(Engine& engine) { engine.Skip(1); };

static_assert(std::is_standard_layout_v<RandomKey>);
static_assert(std::is_trivially_copyable_v<RandomKey>);
static_assert(std::is_standard_layout_v<RandomCounter>);
static_assert(std::is_trivially_copyable_v<RandomCounter>);
static_assert(RandomKey{} == RandomKey{0});
static_assert(RandomCounter{} == RandomCounter{0, 0});
constexpr RandomKey kDefaultKey = [] {
  RandomKey key;
  return key;
}();
constexpr RandomCounter kDefaultCounter = [] {
  RandomCounter counter;
  return counter;
}();
static_assert(kDefaultKey == RandomKey{0});
static_assert(kDefaultCounter == RandomCounter{0, 0});
static_assert(!HasSeed<Philox4x32_10>);
static_assert(!HasReset<Philox4x32_10>);
static_assert(!HasSkip<Philox4x32_10>);

struct PhiloxVector {
  RandomKey key;
  RandomCounter counter;
  Philox4x32_10::ResultType expected;
};

// These constants were calculated from the published Random123 round
// equations, independently of the ASC implementation.
constexpr std::array<PhiloxVector, 9> kVectors{{
    {{0x0000000000000000ULL},
     {0x0000000000000000ULL, 0x0000000000000000ULL},
     {0x6627e8d5U, 0xe169c58dU, 0xbc57ac4cU, 0x9b00dbd8U}},
    {{0x0000000000000000ULL},
     {0x0000000000000000ULL, 0x0000000000000001ULL},
     {0xf8e4cca4U, 0x5cb200dbU, 0xb1a574ebU, 0x097eff67U}},
    {{0x0000000000000000ULL},
     {0x0000000000000000ULL, 0x0000000000000002ULL},
     {0x04faa329U, 0x51c732a6U, 0x241513adU, 0x459135e4U}},
    {{0x0000000000000000ULL},
     {0x0000000000000000ULL, 0x0000000000000003ULL},
     {0xc990ef29U, 0x6a4474a6U, 0x9ac9134fU, 0x6d413e04U}},
    {{0x0123456789abcdefULL},
     {0x0000000000000000ULL, 0x0000000000000000ULL},
     {0xb850222eU, 0xc58cb04bU, 0x14a7a020U, 0x7a84fff9U}},
    {{0x0000000000000000ULL},
     {0x0123456789abcdefULL, 0x0000000000000000ULL},
     {0x1083223bU, 0xa4f40e0fU, 0xea05c91eU, 0x6e7c329eU}},
    {{0xfedcba9876543210ULL},
     {0x0123456789abcdefULL, 0x0fedcba987654321ULL},
     {0xd50e1267U, 0xc3ef480fU, 0x54a0979bU, 0x4f87b798U}},
    {{0x0000000000000000ULL},
     {0x0000000000000000ULL, 0xffffffffffffffffULL},
     {0xf3ce744dU, 0xdfb9980fU, 0x5a7caad1U, 0x25d14252U}},
    {{0xffffffffffffffffULL},
     {0xffffffffffffffffULL, 0xffffffffffffffffULL},
     {0x408f276dU, 0x41c83b0eU, 0xa20bc7c6U, 0x6d5451fdU}},
}};

TEST(PhiloxEngineTest, MatchesIndependentRawWordVectors) {
  for (const PhiloxVector& vector : kVectors) {
    EXPECT_EQ(Philox4x32_10::Generate(vector.key, vector.counter),
              vector.expected);
  }
}

TEST(PhiloxEngineTest, Generate64UsesFirstTwoWordsInFrozenOrder) {
  for (const PhiloxVector& vector : kVectors) {
    const std::uint64_t expected =
        (static_cast<std::uint64_t>(vector.expected[0]) << 32U) |
        static_cast<std::uint64_t>(vector.expected[1]);
    EXPECT_EQ(Philox4x32_10::Generate64(vector.key, vector.counter),
              expected);
  }
}

TEST(PhiloxEngineTest, SelectionIsPureAndIndependentOfCallOrder) {
  constexpr RandomKey kKey{0x0123456789abcdefULL};
  constexpr RandomCounter kFirst{9, 11};
  constexpr RandomCounter kSecond{9, 12};
  const auto first = Philox4x32_10::Generate(kKey, kFirst);
  const auto second = Philox4x32_10::Generate(kKey, kSecond);

  EXPECT_EQ(Philox4x32_10::Generate(kKey, kSecond), second);
  EXPECT_EQ(Philox4x32_10::Generate(kKey, kFirst), first);
  EXPECT_NE(first, second);
  EXPECT_NE(Philox4x32_10::Generate(RandomKey{kKey.value + 1}, kFirst),
            first);
  EXPECT_NE(Philox4x32_10::Generate(
                kKey, RandomCounter{kFirst.subsequence + 1, kFirst.offset}),
            first);
}

}  // namespace
}  // namespace asc
