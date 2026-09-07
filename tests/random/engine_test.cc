#include "asc/random/engine.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/status.h"
#include "test_support.h"

namespace {

using Counter = asc::Philox4x32Counter;
using Key = asc::Philox4x32Key;
using Result = asc::Philox4x32Result;

static_assert(std::is_same_v<Counter, std::array<std::uint32_t, 4>>);
static_assert(std::is_same_v<Key, std::array<std::uint32_t, 2>>);
static_assert(std::is_same_v<Result, std::array<std::uint32_t, 4>>);
static_assert(std::is_same_v<asc::RandomStream, std::uint64_t>);
static_assert(std::is_same_v<asc::RandomSubsequence, std::uint64_t>);
static_assert(std::is_same_v<asc::RandomOffset, std::uint64_t>);
static_assert(noexcept(asc::Philox4x32_10(Counter{}, Key{})));
static_assert(noexcept(asc::GeneratePhilox4x32Block(0, 0, 0)));
static_assert(noexcept(asc::GeneratePhilox4x32Word(0, 0, 0)));

struct BlockVector {
  Counter counter;
  Key key;
  Result expected;
};

constexpr std::array<BlockVector, 9> kBlockVectors = {{
    {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0x00000000U, 0x00000000U},
     {0x6627E8D5U, 0xE169C58DU, 0xBC57AC4CU, 0x9B00DBD8U}},
    {{0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU},
     {0xFFFFFFFFU, 0xFFFFFFFFU},
     {0x408F276DU, 0x41C83B0EU, 0xA20BC7C6U, 0x6D5451FDU}},
    {{0x01234567U, 0x89ABCDEFU, 0xFEDCBA98U, 0x76543210U},
     {0x13579BDFU, 0x2468ACE0U},
     {0xF3B36D22U, 0x1C1759F9U, 0xAD23A12EU, 0x11413B1CU}},
    {{0x00000001U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0x00000000U, 0x00000000U},
     {0xF8E4CCA4U, 0x5CB200DBU, 0xB1A574EBU, 0x097EFF67U}},
    {{0x00000000U, 0x00000001U, 0x00000000U, 0x00000000U},
     {0x00000000U, 0x00000000U},
     {0x6AD0C5ECU, 0xEA236249U, 0x73A459F5U, 0x074944B3U}},
    {{0x00000000U, 0x00000000U, 0x00000001U, 0x00000000U},
     {0x00000000U, 0x00000000U},
     {0x844515E1U, 0xF08D6EAAU, 0x0F19C053U, 0x83F875F0U}},
    {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000001U},
     {0x00000000U, 0x00000000U},
     {0x2DCE73E5U, 0x1348E23FU, 0xFCF8E0ECU, 0xA287AADBU}},
    {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0x00000001U, 0x00000000U},
     {0xE3E80670U, 0xE50A0EBCU, 0x95F222C0U, 0xB615AA27U}},
    {{0x00000000U, 0x00000000U, 0x00000000U, 0x00000000U},
     {0x00000000U, 0x00000001U},
     {0xFDDE3E0BU, 0xFA7E58B6U, 0x3380EC46U, 0xD8D55C4FU}},
}};

void CheckIndependentBlockVectors(asc_random_test::TestContext& context) {
  for (const BlockVector& vector : kBlockVectors) {
    const Counter original_counter = vector.counter;
    const Key original_key = vector.key;
    const Result first = asc::Philox4x32_10(vector.counter, vector.key);
    const Result second = asc::Philox4x32_10(vector.counter, vector.key);
    ASC_RANDOM_TEST_EQ(context, first, vector.expected);
    ASC_RANDOM_TEST_EQ(context, second, vector.expected);
    ASC_RANDOM_TEST_EQ(context, vector.counter, original_counter);
    ASC_RANDOM_TEST_EQ(context, vector.key, original_key);
  }
}

void CheckStreamSubsequenceAndBlockMapping(
    asc_random_test::TestContext& context) {
  constexpr asc::RandomStream kStream = 0x0123456789ABCDEFULL;
  constexpr asc::RandomSubsequence kSubsequence = 0xFEDCBA9876543210ULL;

  const Result block_zero =
      asc::GeneratePhilox4x32Block(kStream, kSubsequence, std::uint64_t{0});
  const Result expected_zero = {0xAEF2ADF7U, 0xF69B5950U, 0x3CEB44F4U,
                                0x89B6573AU};
  ASC_RANDOM_TEST_EQ(context, block_zero, expected_zero);

  const Result block_one =
      asc::GeneratePhilox4x32Block(kStream, kSubsequence, std::uint64_t{1});
  const Result expected_one = {0xEC2AB39FU, 0x4671FD85U, 0x74DECAE0U,
                               0x4B77EC76U};
  ASC_RANDOM_TEST_EQ(context, block_one, expected_one);

  const Result maximum_block = asc::GeneratePhilox4x32Block(
      kStream, kSubsequence, std::numeric_limits<std::uint64_t>::max());
  const Result expected_maximum = {0x18A86018U, 0x11CD6D66U, 0xE208B298U,
                                   0x75C8B351U};
  ASC_RANDOM_TEST_EQ(context, maximum_block, expected_maximum);

  ASC_RANDOM_TEST_CHECK(
      context,
      asc::GeneratePhilox4x32Block(kStream + 1, kSubsequence, 0) != block_zero);
  ASC_RANDOM_TEST_CHECK(
      context,
      asc::GeneratePhilox4x32Block(kStream, kSubsequence + 1, 0) != block_zero);
}

void CheckWordOffsetMapping(asc_random_test::TestContext& context) {
  constexpr asc::RandomStream kStream = 0x0123456789ABCDEFULL;
  constexpr asc::RandomSubsequence kSubsequence = 0xFEDCBA9876543210ULL;

  struct WordVector {
    asc::RandomOffset offset;
    std::uint32_t expected;
  };
  constexpr std::array<WordVector, 6> kWordVectors = {{
      {0x0000000000000000ULL, 0xAEF2ADF7U},
      {0x0000000000000001ULL, 0xF69B5950U},
      {0x0000000000000003ULL, 0x89B6573AU},
      {0x0000000000000004ULL, 0xEC2AB39FU},
      {0x0000000000000005ULL, 0x4671FD85U},
      {0xFFFFFFFFFFFFFFFFULL, 0x4742B6C6U},
  }};

  for (const WordVector& vector : kWordVectors) {
    ASC_RANDOM_TEST_EQ(
        context,
        asc::GeneratePhilox4x32Word(kStream, kSubsequence, vector.offset),
        vector.expected);
  }

  for (asc::RandomOffset offset = 0; offset != 8; ++offset) {
    const Result block =
        asc::GeneratePhilox4x32Block(kStream, kSubsequence, offset / 4);
    ASC_RANDOM_TEST_EQ(
        context, asc::GeneratePhilox4x32Word(kStream, kSubsequence, offset),
        block[static_cast<std::size_t>(offset % 4)]);
  }
}

void CheckOffsetAdvance(asc_random_test::TestContext& context) {
  constexpr auto kMaximum = std::numeric_limits<asc::RandomOffset>::max();
  ASC_RANDOM_TEST_EQ(context, *asc::AdvanceRandomOffset(0, 0), 0U);
  ASC_RANDOM_TEST_EQ(context, *asc::AdvanceRandomOffset(0, kMaximum), kMaximum);
  ASC_RANDOM_TEST_EQ(context, *asc::AdvanceRandomOffset(kMaximum, 0), kMaximum);
  ASC_RANDOM_TEST_EQ(context, *asc::AdvanceRandomOffset(kMaximum - 3, 3),
                     kMaximum);
  ASC_RANDOM_TEST_EQ(context,
                     asc::AdvanceRandomOffset(kMaximum, 1).status().code(),
                     asc::ErrorCode::kOverflow);
  ASC_RANDOM_TEST_EQ(context,
                     asc::AdvanceRandomOffset(kMaximum - 2, 3).status().code(),
                     asc::ErrorCode::kOverflow);
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckIndependentBlockVectors(context);
  CheckStreamSubsequenceAndBlockMapping(context);
  CheckWordOffsetMapping(context);
  CheckOffsetAdvance(context);
  return context.Finish();
}
