#include "asc/random/engine.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/status.h"
#include "reference_philox.h"
#include "test_support.h"

namespace {

struct RawVector {
  std::string_view name;
  asc::Philox4x32Counter counter;
  asc::Philox4x32Key key;
  asc::Philox4x32Result expected;
};

constexpr std::array<RawVector, 10> kIndependentVectors = {{
    {
        "zero",
        {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U}},
        {{0x00000000U, 0x00000000U}},
        {0x6627e8d5U, 0xe169c58dU, 0xbc57ac4cU, 0x9b00dbd8U},
    },
    {
        "all one",
        {{0xffffffffU, 0xffffffffU, 0xffffffffU, 0xffffffffU}},
        {{0xffffffffU, 0xffffffffU}},
        {0x408f276dU, 0x41c83b0eU, 0xa20bc7c6U, 0x6d5451fdU},
    },
    {
        "asymmetric",
        {{0x01234567U, 0x89abcdefU, 0xfedcba98U, 0x76543210U}},
        {{0x13579bdfU, 0x2468ace0U}},
        {0xf3b36d22U, 0x1c1759f9U, 0xad23a12eU, 0x11413b1cU},
    },
    {
        "c0",
        {{0x00000001U, 0x00000000U, 0x00000000U, 0x00000000U}},
        {{0x00000000U, 0x00000000U}},
        {0xf8e4cca4U, 0x5cb200dbU, 0xb1a574ebU, 0x097eff67U},
    },
    {
        "c1",
        {{0x00000000U, 0x00000001U, 0x00000000U, 0x00000000U}},
        {{0x00000000U, 0x00000000U}},
        {0x6ad0c5ecU, 0xea236249U, 0x73a459f5U, 0x074944b3U},
    },
    {
        "c2",
        {{0x00000000U, 0x00000000U, 0x00000001U, 0x00000000U}},
        {{0x00000000U, 0x00000000U}},
        {0x844515e1U, 0xf08d6eaaU, 0x0f19c053U, 0x83f875f0U},
    },
    {
        "c3",
        {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000001U}},
        {{0x00000000U, 0x00000000U}},
        {0x2dce73e5U, 0x1348e23fU, 0xfcf8e0ecU, 0xa287aadbU},
    },
    {
        "k0",
        {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U}},
        {{0x00000001U, 0x00000000U}},
        {0xe3e80670U, 0xe50a0ebcU, 0x95f222c0U, 0xb615aa27U},
    },
    {
        "k1",
        {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U}},
        {{0x00000000U, 0x00000001U}},
        {0xfdde3e0bU, 0xfa7e58b6U, 0x3380ec46U, 0xd8d55c4fU},
    },
    {
        "lane mapping",
        {{0x55667788U, 0x11223344U, 0xddeeff00U, 0x99aabbccU}},
        {{0x05060708U, 0x01020304U}},
        {0xac7db954U, 0x73017cb1U, 0x1bf18b86U, 0x3312161eU},
    },
}};

void CheckRawVectors(asc_random_test::TestContext& context) {
  for (const RawVector& vector : kIndependentVectors) {
    static_cast<void>(vector.name);
    const auto actual = asc::Philox4x32_10(vector.counter, vector.key);
    ASC_RANDOM_TEST_EQ(context, actual, vector.expected);

    const asc_random_test::ReferenceCounter counter = vector.counter.words;
    const asc_random_test::ReferenceKey key = vector.key.words;
    const auto independently_calculated =
        asc_random_test::ReferencePhilox4x32_10(counter, key);
    ASC_RANDOM_TEST_EQ(context, independently_calculated, vector.expected);
    ASC_RANDOM_TEST_EQ(context, actual, independently_calculated);
  }
}

void CheckStreamSubsequenceAndBlockMapping(
    asc_random_test::TestContext& context) {
  constexpr asc::RandomStream kStream = 0x0102030405060708ULL;
  constexpr asc::RandomSubsequence kSubsequence = 0x99aabbccddeeff00ULL;
  constexpr std::uint64_t kBlock = 0x1122334455667788ULL;

  const asc::Philox4x32Counter expected_counter{{
      0x55667788U,
      0x11223344U,
      0xddeeff00U,
      0x99aabbccU,
  }};
  const asc::Philox4x32Key expected_key{{
      0x05060708U,
      0x01020304U,
  }};
  const auto direct = asc::Philox4x32_10(expected_counter, expected_key);
  const auto mapped = asc::Philox4x32Block(kStream, kSubsequence, kBlock);
  ASC_RANDOM_TEST_EQ(context, mapped, direct);
  ASC_RANDOM_TEST_EQ(context, mapped, kIndependentVectors.back().expected);
}

void CheckWordOffsets(asc_random_test::TestContext& context) {
  constexpr asc::RandomStream kStream = 0x1020304050607080ULL;
  constexpr asc::RandomSubsequence kSubsequence = 0x90a0b0c0d0e0f000ULL;

  for (asc::RandomOffset offset = 0; offset < 20; ++offset) {
    const auto block = asc::Philox4x32Block(kStream, kSubsequence, offset / 4U);
    ASC_RANDOM_TEST_EQ(context,
                       asc::Philox4x32Word(kStream, kSubsequence, offset),
                       block[static_cast<std::size_t>(offset % 4U)]);
  }

  constexpr asc::RandomOffset kMappedOffset = 0x4488cd115599de22ULL;
  const auto mapped_block = asc::Philox4x32Block(
      0x0102030405060708ULL, 0x99aabbccddeeff00ULL, kMappedOffset / 4U);
  ASC_RANDOM_TEST_EQ(context,
                     asc::Philox4x32Word(0x0102030405060708ULL,
                                         0x99aabbccddeeff00ULL, kMappedOffset),
                     mapped_block[2]);
  ASC_RANDOM_TEST_EQ(context, mapped_block,
                     kIndependentVectors.back().expected);
}

void CheckOffsetAdvance(asc_random_test::TestContext& context) {
  constexpr asc::RandomOffset kMaximum =
      std::numeric_limits<asc::RandomOffset>::max();

  const auto unchanged = asc::AdvanceRandomOffset(17, 0);
  ASC_RANDOM_TEST_CHECK(context, unchanged.ok());
  ASC_RANDOM_TEST_EQ(context, *unchanged, asc::RandomOffset{17});

  const auto ordinary = asc::AdvanceRandomOffset(17, 25);
  ASC_RANDOM_TEST_CHECK(context, ordinary.ok());
  ASC_RANDOM_TEST_EQ(context, *ordinary, asc::RandomOffset{42});

  const auto exact = asc::AdvanceRandomOffset(kMaximum - 8, 8);
  ASC_RANDOM_TEST_CHECK(context, exact.ok());
  ASC_RANDOM_TEST_EQ(context, *exact, kMaximum);

  const auto overflow = asc::AdvanceRandomOffset(kMaximum, 1);
  ASC_RANDOM_TEST_CHECK(context, !overflow.ok());
  ASC_RANDOM_TEST_EQ(context, overflow.status().code(),
                     asc::ErrorCode::kOverflow);

  const auto larger_overflow = asc::AdvanceRandomOffset(kMaximum - 8, 9);
  ASC_RANDOM_TEST_CHECK(context, !larger_overflow.ok());
  ASC_RANDOM_TEST_EQ(context, larger_overflow.status().code(),
                     asc::ErrorCode::kOverflow);
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckRawVectors(context);
  CheckStreamSubsequenceAndBlockMapping(context);
  CheckWordOffsets(context);
  CheckOffsetAdvance(context);
  return context.Finish();
}
