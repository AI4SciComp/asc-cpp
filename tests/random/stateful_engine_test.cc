#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/core/status.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "test_support.h"

namespace {

static_assert(asc::CanonicalRandomEngine<asc::SplitMix64>);
static_assert(asc::CanonicalRandomEngine<asc::Pcg32>);
static_assert(asc::CanonicalRandomEngine<asc::Xoroshiro64Star>);
static_assert(asc::CanonicalRandomEngine<asc::Xoroshiro128Plus>);
static_assert(std::is_trivially_copyable_v<asc::SplitMix64>);
static_assert(std::is_trivially_copyable_v<asc::Pcg32>);
static_assert(std::is_trivially_copyable_v<asc::Xoroshiro64Star>);
static_assert(std::is_trivially_copyable_v<asc::Xoroshiro128Plus>);
static_assert(!std::is_default_constructible_v<asc::SplitMix64>);
static_assert(!std::is_default_constructible_v<asc::Pcg32>);
static_assert(!std::is_default_constructible_v<asc::Xoroshiro64Star>);
static_assert(!std::is_default_constructible_v<asc::Xoroshiro128Plus>);
static_assert(asc::SplitMix64::min() == 0);
static_assert(asc::SplitMix64::max() ==
              std::numeric_limits<std::uint64_t>::max());
static_assert(asc::Pcg32::min() == 0);
static_assert(asc::Pcg32::max() == std::numeric_limits<std::uint32_t>::max());

void CheckSplitMix64(asc_random_test::TestContext& context) {
  constexpr std::array<std::uint64_t, 6> kSeedZero = {
      0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL, 0x06C45D188009454FULL,
      0xF88BB8A8724C81ECULL, 0x1B39896A51A8749BULL, 0x53CB9F0C747EA2EAULL};
  asc::SplitMix64 engine(0);
  for (const std::uint64_t expected : kSeedZero) {
    ASC_RANDOM_TEST_EQ(context, engine(), expected);
  }
  ASC_RANDOM_TEST_EQ(context, engine.ExportState(),
                     (asc::SplitMix64State{asc::kRandomSequenceVersion1,
                                           0xB54CDA58FBBEE87EULL}));

  asc::SplitMix64 extreme(std::numeric_limits<std::uint64_t>::max());
  ASC_RANDOM_TEST_EQ(context, extreme(), 0xE4D971771B652C20ULL);
  ASC_RANDOM_TEST_EQ(context, extreme(), 0xE99FF867DBF682C9ULL);
  ASC_RANDOM_TEST_EQ(context, extreme(), 0x382FF84CB27281E9ULL);

  const asc::SplitMix64State saved = engine.ExportState();
  auto restored = asc::SplitMix64::FromState(saved);
  ASC_RANDOM_TEST_CHECK(context, restored.ok());
  ASC_RANDOM_TEST_EQ(context, engine(), (*restored)());

  asc::SplitMix64 invalid_target(91);
  const asc::SplitMix64State before = invalid_target.ExportState();
  ASC_RANDOM_TEST_EQ(
      context,
      invalid_target.RestoreState({asc::kRandomSequenceVersion1 + 1U, 123})
          .code(),
      asc::ErrorCode::kVersion);
  ASC_RANDOM_TEST_EQ(context, invalid_target.ExportState(), before);
}

void CheckPcg32(asc_random_test::TestContext& context) {
  constexpr std::array<std::uint32_t, 6> kOfficial = {0xA15C02B7U, 0x7B47F409U,
                                                      0xBA1D3330U, 0x83D2F293U,
                                                      0xBFA4784BU, 0xCBED606EU};
  asc::Pcg32 engine(42, 54);
  for (const std::uint32_t expected : kOfficial) {
    ASC_RANDOM_TEST_EQ(context, engine(), expected);
  }
  ASC_RANDOM_TEST_EQ(context, engine.ExportState(),
                     (asc::Pcg32State{asc::kRandomSequenceVersion1,
                                      0xBEB6D0B73FDB974AULL, 0x6DULL}));

  asc::Pcg32 high_stream_ignored(42, 54ULL | (1ULL << 63U));
  asc::Pcg32 low_stream_only(42, 54);
  for (int index = 0; index < 6; ++index) {
    ASC_RANDOM_TEST_EQ(context, high_stream_ignored(), low_stream_only());
  }

  asc::Pcg32 zero(0, 0);
  constexpr std::array<std::uint32_t, 4> kZero = {0xE4C14788U, 0x379C6516U,
                                                  0x5C4AB3BBU, 0x601D23E0U};
  for (const std::uint32_t expected : kZero) {
    ASC_RANDOM_TEST_EQ(context, zero(), expected);
  }

  asc::Pcg32 extreme(std::numeric_limits<std::uint64_t>::max(),
                     std::numeric_limits<std::uint64_t>::max());
  ASC_RANDOM_TEST_EQ(context, extreme(), 0x2675C047U);
  ASC_RANDOM_TEST_EQ(context, extreme(), 0x7779A837U);

  const asc::Pcg32State saved = engine.ExportState();
  auto restored = asc::Pcg32::FromState(saved);
  ASC_RANDOM_TEST_CHECK(context, restored.ok());
  ASC_RANDOM_TEST_EQ(context, engine(), (*restored)());

  asc::Pcg32 invalid_target(7);
  const asc::Pcg32State before = invalid_target.ExportState();
  ASC_RANDOM_TEST_EQ(
      context,
      invalid_target.RestoreState({asc::kRandomSequenceVersion1, 123, 10})
          .code(),
      asc::ErrorCode::kInvalidState);
  ASC_RANDOM_TEST_EQ(context, invalid_target.ExportState(), before);
  ASC_RANDOM_TEST_EQ(
      context,
      invalid_target.RestoreState({asc::kRandomSequenceVersion1 + 1U, 123, 11})
          .code(),
      asc::ErrorCode::kVersion);
  ASC_RANDOM_TEST_EQ(context, invalid_target.ExportState(), before);
}

void CheckXoroshiro64Star(asc_random_test::TestContext& context) {
  auto direct =
      asc::Xoroshiro64Star::FromState({asc::kRandomSequenceVersion1, 1, 2});
  ASC_RANDOM_TEST_CHECK(context, direct.ok());
  constexpr std::array<std::uint32_t, 6> kExpected = {0x9E3779BBU, 0x1380CF31U,
                                                      0xF233F6B9U, 0xFDE6B3B9U,
                                                      0x0F9C9E6CU, 0x0A055D19U};
  for (const std::uint32_t expected : kExpected) {
    ASC_RANDOM_TEST_EQ(context, (*direct)(), expected);
  }
  ASC_RANDOM_TEST_EQ(context, direct->ExportState(),
                     (asc::Xoroshiro64StarState{asc::kRandomSequenceVersion1,
                                                0x4A5046B5U, 0x8AA839A6U}));

  auto jumped =
      asc::Xoroshiro64Star::FromState({asc::kRandomSequenceVersion1, 1, 2});
  jumped->Jump();
  ASC_RANDOM_TEST_EQ(context, jumped->ExportState(),
                     (asc::Xoroshiro64StarState{asc::kRandomSequenceVersion1,
                                                0xC8DFB488U, 0x97453862U}));
  auto long_jumped =
      asc::Xoroshiro64Star::FromState({asc::kRandomSequenceVersion1, 1, 2});
  long_jumped->LongJump();
  ASC_RANDOM_TEST_EQ(context, long_jumped->ExportState(),
                     (asc::Xoroshiro64StarState{asc::kRandomSequenceVersion1,
                                                0x346FAFE7U, 0x7AFCF612U}));

  asc::Xoroshiro64Star seeded(0);
  ASC_RANDOM_TEST_EQ(context, seeded.ExportState(),
                     (asc::Xoroshiro64StarState{asc::kRandomSequenceVersion1,
                                                0x7B1DCDAFU, 0xA1B965F4U}));
  const auto before = seeded.ExportState();
  ASC_RANDOM_TEST_EQ(
      context, seeded.RestoreState({asc::kRandomSequenceVersion1, 0, 0}).code(),
      asc::ErrorCode::kInvalidState);
  ASC_RANDOM_TEST_EQ(context, seeded.ExportState(), before);
  ASC_RANDOM_TEST_EQ(
      context,
      asc::Xoroshiro64Star::FromState({asc::kRandomSequenceVersion1 + 1U, 1, 2})
          .status()
          .code(),
      asc::ErrorCode::kVersion);
}

void CheckXoroshiro128Plus(asc_random_test::TestContext& context) {
  auto direct =
      asc::Xoroshiro128Plus::FromState({asc::kRandomSequenceVersion1, 1, 2});
  ASC_RANDOM_TEST_CHECK(context, direct.ok());
  constexpr std::array<std::uint64_t, 6> kExpected = {
      0x0000000000000003ULL, 0x0000006001030003ULL, 0x20C102C302000C03ULL,
      0x810180670D23AD61ULL, 0x26D13A4941333A42ULL, 0x538A501C02F58B2EULL};
  for (const std::uint64_t expected : kExpected) {
    ASC_RANDOM_TEST_EQ(context, (*direct)(), expected);
  }
  ASC_RANDOM_TEST_EQ(context, direct->ExportState(),
                     (asc::Xoroshiro128PlusState{asc::kRandomSequenceVersion1,
                                                 0x0C10A1AC85F62D0EULL,
                                                 0x1EA165C168420270ULL}));

  auto jumped =
      asc::Xoroshiro128Plus::FromState({asc::kRandomSequenceVersion1, 1, 2});
  jumped->Jump();
  ASC_RANDOM_TEST_EQ(context, jumped->ExportState(),
                     (asc::Xoroshiro128PlusState{asc::kRandomSequenceVersion1,
                                                 0x66FBD4BE1DF0A7B5ULL,
                                                 0x830C3DDBB4AA3172ULL}));
  auto long_jumped =
      asc::Xoroshiro128Plus::FromState({asc::kRandomSequenceVersion1, 1, 2});
  long_jumped->LongJump();
  ASC_RANDOM_TEST_EQ(context, long_jumped->ExportState(),
                     (asc::Xoroshiro128PlusState{asc::kRandomSequenceVersion1,
                                                 0x3CE44494D47D323AULL,
                                                 0x2AA25CA8D61DE643ULL}));

  asc::Xoroshiro128Plus seeded(0);
  ASC_RANDOM_TEST_EQ(context, seeded.ExportState(),
                     (asc::Xoroshiro128PlusState{asc::kRandomSequenceVersion1,
                                                 0xE220A8397B1DCDAFULL,
                                                 0x6E789E6AA1B965F4ULL}));
  const auto before = seeded.ExportState();
  ASC_RANDOM_TEST_EQ(
      context,
      seeded.RestoreState({asc::kRandomSequenceVersion1 + 1U, 5, 7}).code(),
      asc::ErrorCode::kVersion);
  ASC_RANDOM_TEST_EQ(context, seeded.ExportState(), before);
  ASC_RANDOM_TEST_EQ(
      context, seeded.RestoreState({asc::kRandomSequenceVersion1, 0, 0}).code(),
      asc::ErrorCode::kInvalidState);
  ASC_RANDOM_TEST_EQ(context, seeded.ExportState(), before);
}

void CheckValueSemantics(asc_random_test::TestContext& context) {
  asc::Xoroshiro128Plus original(19);
  asc::Xoroshiro128Plus copied = original;
  ASC_RANDOM_TEST_EQ(context, original(), copied());
  ASC_RANDOM_TEST_EQ(context, original(), copied());

  asc::Xoroshiro128Plus moved = std::move(copied);
  asc::Xoroshiro128Plus copied_after_move = copied;
  ASC_RANDOM_TEST_EQ(context, moved(), copied_after_move());
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckSplitMix64(context);
  CheckPcg32(context);
  CheckXoroshiro64Star(context);
  CheckXoroshiro128Plus(context);
  CheckValueSemantics(context);
  return context.Finish();
}
