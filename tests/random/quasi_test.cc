#include "asc/random/quasi.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <thread>
#include <type_traits>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/status.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "test_support.h"

namespace {

class CountingEngine final {
 public:
  using result_type = std::uint64_t;

  [[nodiscard]] static constexpr result_type min() noexcept { return 0; }
  [[nodiscard]] static constexpr result_type max() noexcept {
    return std::numeric_limits<result_type>::max();
  }
  [[nodiscard]] result_type operator()() noexcept {
    ++calls;
    return max();
  }

  std::size_t calls = 0;
};

static_assert(asc::CanonicalRandomEngine<CountingEngine>);
static_assert(std::is_trivially_copyable_v<asc::SobolSequence>);

void CheckPrimeAndRadicalInverse(asc_random_test::TestContext& context) {
  constexpr std::array<std::uint32_t, 10> kFirstPrimes = {2,  3,  5,  7,  11,
                                                          13, 17, 19, 23, 29};
  for (std::size_t index = 0; index < kFirstPrimes.size(); ++index) {
    const auto prime = asc::PrimeAt(index);
    ASC_RANDOM_TEST_CHECK(context, prime.ok());
    ASC_RANDOM_TEST_EQ(context, *prime, kFirstPrimes[index]);
  }
  const auto last_prime = asc::PrimeAt(asc::kSobolDimensionCount - 1);
  ASC_RANDOM_TEST_CHECK(context, last_prime.ok());
  ASC_RANDOM_TEST_EQ(context, *last_prime, std::uint32_t{239737});
  ASC_RANDOM_TEST_EQ(context,
                     asc::PrimeAt(asc::kSobolDimensionCount).status().code(),
                     asc::ErrorCode::kIndex);

  constexpr std::array<double, 8> kBaseTwo = {0.0,   0.5,   0.25,  0.75,
                                              0.125, 0.625, 0.375, 0.875};
  for (std::uint64_t index = 0; index < kBaseTwo.size(); ++index) {
    const auto value = asc::RadicalInverse<double>(2, index);
    ASC_RANDOM_TEST_CHECK(context, value.ok());
    ASC_RANDOM_TEST_EQ(context, *value, kBaseTwo[index]);
  }
  ASC_RANDOM_TEST_EQ(context, asc::RadicalInverse<double>(1, 4).status().code(),
                     asc::ErrorCode::kInvalidArgument);
  const auto extreme =
      asc::RadicalInverse<float>(2, std::numeric_limits<std::uint64_t>::max());
  ASC_RANDOM_TEST_CHECK(context, extreme.ok());
  ASC_RANDOM_TEST_EQ(context, *extreme, std::nextafter(1.0F, 0.0F));

  constexpr std::array<std::uint32_t, 3> kPermutation = {0, 2, 1};
  const auto scrambled =
      asc::ScrambledRadicalInverse<double>(3, 1, kPermutation);
  ASC_RANDOM_TEST_CHECK(context, scrambled.ok());
  ASC_RANDOM_TEST_EQ(context, *scrambled, 2.0 / 3.0);
  constexpr std::array<std::uint32_t, 3> kRepeated = {0, 1, 1};
  constexpr std::array<std::uint32_t, 3> kMovedZero = {1, 0, 2};
  constexpr std::array<std::uint32_t, 3> kOutOfRange = {0, 1, 3};
  constexpr std::array<std::uint32_t, 2> kShort = {0, 1};
  ASC_RANDOM_TEST_EQ(
      context,
      asc::ScrambledRadicalInverse<double>(3, 1, kRepeated).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(
      context,
      asc::ScrambledRadicalInverse<double>(3, 1, kMovedZero).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(
      context,
      asc::ScrambledRadicalInverse<double>(3, 1, kOutOfRange).status().code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(
      context,
      asc::ScrambledRadicalInverse<double>(3, 1, kShort).status().code(),
      asc::ErrorCode::kShape);
}

// Permutation and Latin-hypercube invariants share engine-state checks.
// NOLINTNEXTLINE(readability-function-size)
void CheckPermutationAndLatin(asc_random_test::TestContext& context) {
  CountingEngine permutation_engine;
  std::array<std::uint32_t, 8> permutation{};
  ASC_RANDOM_TEST_CHECK(context, asc::GenerateLowDiscrepancyPermutation(
                                     permutation_engine, permutation)
                                     .ok());
  ASC_RANDOM_TEST_EQ(context, permutation_engine.calls, std::size_t{7});
  const auto first_permutation = permutation;
  std::sort(permutation.begin(), permutation.end());
  for (std::size_t index = 0; index < permutation.size(); ++index) {
    ASC_RANDOM_TEST_EQ(context, permutation[index], index);
  }
  CountingEngine repeat_permutation_engine;
  std::array<std::uint32_t, 8> repeated_permutation{};
  ASC_RANDOM_TEST_CHECK(
      context, asc::GenerateLowDiscrepancyPermutation(repeat_permutation_engine,
                                                      repeated_permutation)
                   .ok());
  ASC_RANDOM_TEST_EQ(context, repeat_permutation_engine.calls,
                     permutation_engine.calls);
  ASC_RANDOM_TEST_EQ(context, repeated_permutation, first_permutation);

  CountingEngine midpoint_engine;
  std::array<std::uint32_t, 8> midpoint_workspace{};
  std::array<double, 8> midpoints{};
  ASC_RANDOM_TEST_CHECK(
      context, asc::GenerateLatinHypercubeMidpoints<double>(
                   4, 2, midpoint_engine, midpoint_workspace, midpoints)
                   .ok());
  ASC_RANDOM_TEST_EQ(context, midpoint_engine.calls, std::size_t{6});
  for (std::size_t dimension = 0; dimension < 2; ++dimension) {
    std::array<bool, 4> seen{};
    for (std::size_t sample = 0; sample < 4; ++sample) {
      const double value = midpoints[sample * 2 + dimension];
      ASC_RANDOM_TEST_CHECK(context, value >= 0 && value < 1);
      const auto stratum = static_cast<std::size_t>(value * 4);
      ASC_RANDOM_TEST_CHECK(context, stratum < seen.size());
      ASC_RANDOM_TEST_CHECK(context, !seen[stratum]);
      seen[stratum] = true;
      ASC_RANDOM_TEST_EQ(context, value,
                         (static_cast<double>(stratum) + 0.5) / 4.0);
    }
  }

  CountingEngine jitter_engine;
  std::array<std::uint32_t, 8> jitter_workspace{};
  std::array<double, 8> jittered{};
  ASC_RANDOM_TEST_CHECK(context,
                        asc::GenerateLatinHypercubeJittered<double>(
                            4, 2, jitter_engine, jitter_workspace, jittered)
                            .ok());
  ASC_RANDOM_TEST_EQ(context, jitter_engine.calls, std::size_t{14});
  for (std::size_t dimension = 0; dimension < 2; ++dimension) {
    std::array<bool, 4> seen{};
    for (std::size_t sample = 0; sample < 4; ++sample) {
      const double value = jittered[sample * 2 + dimension];
      ASC_RANDOM_TEST_CHECK(context, value >= 0 && value < 1);
      const auto stratum = static_cast<std::size_t>(value * 4);
      ASC_RANDOM_TEST_CHECK(context, stratum < seen.size());
      ASC_RANDOM_TEST_CHECK(context, !seen[stratum]);
      seen[stratum] = true;
    }
  }
  CountingEngine repeat_jitter_engine;
  std::array<std::uint32_t, 8> repeat_jitter_workspace{};
  std::array<double, 8> repeated_jitter{};
  ASC_RANDOM_TEST_CHECK(context, asc::GenerateLatinHypercubeJittered<double>(
                                     4, 2, repeat_jitter_engine,
                                     repeat_jitter_workspace, repeated_jitter)
                                     .ok());
  ASC_RANDOM_TEST_EQ(context, repeat_jitter_engine.calls, jitter_engine.calls);
  ASC_RANDOM_TEST_EQ(context, repeat_jitter_workspace, jitter_workspace);
  ASC_RANDOM_TEST_EQ(context, repeated_jitter, jittered);

  CountingEngine float_jitter_engine;
  std::array<std::uint32_t, 4> float_jitter_workspace{};
  std::array<float, 4> float_jitter{};
  ASC_RANDOM_TEST_CHECK(context, asc::GenerateLatinHypercubeJittered<float>(
                                     4, 1, float_jitter_engine,
                                     float_jitter_workspace, float_jitter)
                                     .ok());
  std::array<bool, 4> float_strata{};
  for (const float value : float_jitter) {
    ASC_RANDOM_TEST_CHECK(context, value >= 0 && value < 1);
    const auto stratum = static_cast<std::size_t>(value * 4);
    ASC_RANDOM_TEST_CHECK(context, stratum < float_strata.size());
    ASC_RANDOM_TEST_CHECK(context, !float_strata[stratum]);
    float_strata[stratum] = true;
  }

  CountingEngine empty_engine;
  ASC_RANDOM_TEST_CHECK(context, asc::GenerateLatinHypercubeJittered<double>(
                                     0, 4, empty_engine, {}, {})
                                     .ok());
  ASC_RANDOM_TEST_EQ(context, empty_engine.calls, std::size_t{0});

  CountingEngine invalid_engine;
  std::array<std::uint32_t, 4> invalid_workspace = {7, 7, 7, 7};
  std::array<double, 3> invalid_output = {9, 9, 9};
  ASC_RANDOM_TEST_EQ(
      context,
      asc::GenerateLatinHypercubeMidpoints<double>(
          2, 2, invalid_engine, invalid_workspace, invalid_output)
          .code(),
      asc::ErrorCode::kShape);
  ASC_RANDOM_TEST_EQ(context, invalid_engine.calls, std::size_t{0});
  ASC_RANDOM_TEST_EQ(context, invalid_workspace[0], std::uint32_t{7});
  ASC_RANDOM_TEST_EQ(context, invalid_output[0], 9.0);

  alignas(double) std::array<std::uint32_t, 8> aliased_storage{};
  std::span<double> aliased_output(
      reinterpret_cast<double*>(aliased_storage.data()), 4);
  ASC_RANDOM_TEST_EQ(
      context,
      asc::GenerateLatinHypercubeMidpoints<double>(
          2, 2, invalid_engine,
          std::span<std::uint32_t>(aliased_storage.data(), 4), aliased_output)
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context, invalid_engine.calls, std::size_t{0});
}

// Both low-discrepancy sequences share boundary and allocation checks.
// NOLINTNEXTLINE(readability-function-size)
void CheckHaltonAndHammersley(asc_random_test::TestContext& context) {
  constexpr std::array<std::array<double, 2>, 5> kHalton = {
      {{0.0, 0.0},
       {0.5, 1.0 / 3.0},
       {0.25, 2.0 / 3.0},
       {0.75, 1.0 / 9.0},
       {0.125, 4.0 / 9.0}}};
  for (std::size_t index = 0; index < kHalton.size(); ++index) {
    std::array<double, 2> point{};
    ASC_RANDOM_TEST_CHECK(context,
                          asc::GenerateHaltonPoint<double>(index, point).ok());
    ASC_RANDOM_TEST_EQ(context, point, kHalton[index]);
  }
  std::array<float, 2> float_halton{};
  ASC_RANDOM_TEST_CHECK(context,
                        asc::GenerateHaltonPoint<float>(3, float_halton).ok());
  ASC_RANDOM_TEST_EQ(context, float_halton,
                     (std::array<float, 2>{0.75F, 0x1.c71c74p-4F}));

  constexpr std::array<std::uint32_t, 2> kBaseTwo = {0, 1};
  constexpr std::array<std::uint32_t, 3> kBaseThree = {0, 2, 1};
  const std::array<std::span<const std::uint32_t>, 2> permutations = {
      std::span<const std::uint32_t>(kBaseTwo),
      std::span<const std::uint32_t>(kBaseThree)};
  std::array<double, 2> scrambled{};
  ASC_RANDOM_TEST_CHECK(context, asc::GenerateScrambledHaltonPoint<double>(
                                     1, permutations, scrambled)
                                     .ok());
  ASC_RANDOM_TEST_EQ(context, scrambled,
                     (std::array<double, 2>{0.5, 2.0 / 3.0}));

  constexpr std::array<std::uint32_t, 3> kInvalidBaseThree = {0, 1, 1};
  const std::array<std::span<const std::uint32_t>, 2> invalid_permutations = {
      std::span<const std::uint32_t>(kBaseTwo),
      std::span<const std::uint32_t>(kInvalidBaseThree)};
  std::array<double, 2> untouched_scrambled = {9, 9};
  ASC_RANDOM_TEST_EQ(context,
                     asc::GenerateScrambledHaltonPoint<double>(
                         1, invalid_permutations, untouched_scrambled)
                         .code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context, untouched_scrambled,
                     (std::array<double, 2>{9, 9}));

  alignas(double) std::array<std::uint32_t, 4> aliased_storage = {0, 1, 7, 7};
  const std::array<std::span<const std::uint32_t>, 1> aliased_permutations = {
      std::span<const std::uint32_t>(aliased_storage.data(), 2)};
  const auto aliased_before = aliased_storage;
  std::span<double> aliased_output(
      reinterpret_cast<double*>(aliased_storage.data()), 1);
  ASC_RANDOM_TEST_EQ(context,
                     asc::GenerateScrambledHaltonPoint<double>(
                         1, aliased_permutations, aliased_output)
                         .code(),
                     asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context, aliased_storage, aliased_before);

  constexpr std::array<std::array<double, 2>, 4> kHammersley = {
      {{0.0, 0.0}, {0.25, 0.5}, {0.5, 0.25}, {0.75, 0.75}}};
  for (std::size_t index = 0; index < kHammersley.size(); ++index) {
    std::array<double, 2> point{};
    ASC_RANDOM_TEST_CHECK(
        context, asc::GenerateHammersleyPoint<double>(index, 4, point).ok());
    ASC_RANDOM_TEST_EQ(context, point, kHammersley[index]);
  }
  std::array<float, 2> float_hammersley{};
  ASC_RANDOM_TEST_CHECK(
      context,
      asc::GenerateHammersleyPoint<float>(1, 4, float_hammersley).ok());
  ASC_RANDOM_TEST_EQ(context, float_hammersley,
                     (std::array<float, 2>{0.25F, 0.5F}));
  std::array<double, 2> untouched = {9, 9};
  ASC_RANDOM_TEST_EQ(
      context, asc::GenerateHammersleyPoint<double>(0, 0, untouched).code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_RANDOM_TEST_EQ(context, untouched, (std::array<double, 2>{9, 9}));
  ASC_RANDOM_TEST_EQ(
      context, asc::GenerateHammersleyPoint<double>(4, 4, untouched).code(),
      asc::ErrorCode::kIndex);

  std::array<double, 1> endpoint{};
  const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
  ASC_RANDOM_TEST_CHECK(context, asc::GenerateHammersleyPoint<double>(
                                     maximum - 1, maximum, endpoint)
                                     .ok());
  ASC_RANDOM_TEST_EQ(context, endpoint[0], std::nextafter(1.0, 0.0));
}

std::uint64_t RuntimeSobolChecksum() {
  constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  std::uint64_t checksum = kOffsetBasis;
  std::array<std::uint64_t, asc::kSobolDirectionWordCount> directions{};
  for (std::size_t dimension = 0; dimension < asc::kSobolDimensionCount;
       ++dimension) {
    if (!asc::InitializeSobolDirectionNumbers(dimension, directions).ok()) {
      return 0;
    }
    for (const std::uint64_t word : directions) {
      for (unsigned int shift = 0; shift < 64; shift += 8) {
        checksum ^= (word >> shift) & 0xFFU;
        checksum *= kPrime;
      }
    }
  }
  return checksum;
}

// Direction-number validation and sequence checks form one Sobol scenario.
// NOLINTNEXTLINE(readability-function-size)
void CheckSobol(asc_random_test::TestContext& context) {
  std::array<std::uint64_t, asc::kSobolDirectionWordCount> directions{};
  ASC_RANDOM_TEST_CHECK(
      context, asc::InitializeSobolDirectionNumbers(0, directions).ok());
  ASC_RANDOM_TEST_EQ(context, directions[0], 0x8000000000000000ULL);
  ASC_RANDOM_TEST_EQ(context, directions[1], 0x4000000000000000ULL);
  ASC_RANDOM_TEST_CHECK(
      context, asc::InitializeSobolDirectionNumbers(1, directions).ok());
  ASC_RANDOM_TEST_EQ(context, directions[0], 0x8000000000000000ULL);
  ASC_RANDOM_TEST_EQ(context, directions[1], 0xC000000000000000ULL);
  ASC_RANDOM_TEST_EQ(context, directions[2], 0xA000000000000000ULL);
  ASC_RANDOM_TEST_EQ(context, RuntimeSobolChecksum(),
                     asc::SobolDirectionTableChecksum());

  std::array<std::uint64_t, 63> short_workspace{};
  short_workspace.fill(9);
  ASC_RANDOM_TEST_EQ(
      context, asc::InitializeSobolDirectionNumbers(0, short_workspace).code(),
      asc::ErrorCode::kShape);
  ASC_RANDOM_TEST_EQ(context, short_workspace[0], std::uint64_t{9});
  ASC_RANDOM_TEST_EQ(context,
                     asc::InitializeSobolDirectionNumbers(
                         asc::kSobolDimensionCount, directions)
                         .code(),
                     asc::ErrorCode::kIndex);

  constexpr std::array<std::array<double, 3>, 8> kPublished = {
      {{0.0, 0.0, 0.0},
       {0.5, 0.5, 0.5},
       {0.75, 0.25, 0.25},
       {0.25, 0.75, 0.75},
       {0.375, 0.375, 0.625},
       {0.875, 0.875, 0.125},
       {0.625, 0.125, 0.875},
       {0.125, 0.625, 0.375}}};
  for (std::size_t index = 0; index < kPublished.size(); ++index) {
    std::array<double, 3> point{};
    ASC_RANDOM_TEST_CHECK(context,
                          asc::GenerateSobolPoint<double>(index, point).ok());
    ASC_RANDOM_TEST_EQ(context, point, kPublished[index]);
  }
  std::array<float, 3> float_sobol{};
  ASC_RANDOM_TEST_CHECK(context,
                        asc::GenerateSobolPoint<float>(4, float_sobol).ok());
  ASC_RANDOM_TEST_EQ(context, float_sobol,
                     (std::array<float, 3>{0.375F, 0.375F, 0.625F}));
  const auto high_dimension =
      asc::SobolCoordinate<double>(12345, asc::kSobolDimensionCount - 1);
  ASC_RANDOM_TEST_CHECK(context, high_dimension.ok());
  ASC_RANDOM_TEST_CHECK(context, *high_dimension >= 0 && *high_dimension < 1);
  ASC_RANDOM_TEST_EQ(context,
                     asc::SobolCoordinate<double>(0, asc::kSobolDimensionCount)
                         .status()
                         .code(),
                     asc::ErrorCode::kIndex);

  auto sequence = asc::SobolSequence::Create(3);
  ASC_RANDOM_TEST_CHECK(context, sequence.ok());
  for (std::uint64_t index = 0; index < 4; ++index) {
    std::array<double, 3> sequential{};
    std::array<double, 3> indexed{};
    ASC_RANDOM_TEST_CHECK(context, sequence->Next<double>(sequential).ok());
    ASC_RANDOM_TEST_CHECK(context,
                          asc::GenerateSobolPoint<double>(index, indexed).ok());
    ASC_RANDOM_TEST_EQ(context, sequential, indexed);
  }
  ASC_RANDOM_TEST_CHECK(context, sequence->SkipTo(7).ok());
  std::array<double, 3> skipped{};
  ASC_RANDOM_TEST_CHECK(context, sequence->Next<double>(skipped).ok());
  ASC_RANDOM_TEST_EQ(context, skipped, kPublished[7]);
  ASC_RANDOM_TEST_CHECK(context, sequence->Reset().ok());
  ASC_RANDOM_TEST_EQ(context, sequence->index(), std::uint64_t{0});

  ASC_RANDOM_TEST_CHECK(
      context,
      sequence->SkipTo(std::numeric_limits<std::uint64_t>::max()).ok());
  ASC_RANDOM_TEST_EQ(context, sequence->Skip(1).code(),
                     asc::ErrorCode::kOverflow);
  ASC_RANDOM_TEST_EQ(context, sequence->index(),
                     std::numeric_limits<std::uint64_t>::max());
  ASC_RANDOM_TEST_CHECK(context, sequence->Next<double>(skipped).ok());
  ASC_RANDOM_TEST_CHECK(context, sequence->exhausted());
  skipped.fill(9);
  ASC_RANDOM_TEST_EQ(context, sequence->Next<double>(skipped).code(),
                     asc::ErrorCode::kEndOfFile);
  ASC_RANDOM_TEST_EQ(context, skipped[0], 9.0);

  const auto invalid_sequence =
      asc::SobolSequence::Create(asc::kSobolDimensionCount + 1);
  ASC_RANDOM_TEST_EQ(context, invalid_sequence.status().code(),
                     asc::ErrorCode::kShape);
  auto empty_sequence = asc::SobolSequence::Create(0, 42);
  ASC_RANDOM_TEST_CHECK(context, empty_sequence.ok());
  ASC_RANDOM_TEST_CHECK(context, empty_sequence->Next<double>({}).ok());
  ASC_RANDOM_TEST_EQ(context, empty_sequence->index(), std::uint64_t{42});
}

void CheckDiscrepancyAndConcurrency(asc_random_test::TestContext& context) {
  std::array<std::size_t, 64> bins{};
  for (std::uint64_t index = 0; index < 256; ++index) {
    std::array<double, 2> point{};
    ASC_RANDOM_TEST_CHECK(context,
                          asc::GenerateHaltonPoint<double>(index, point).ok());
    const auto x =
        std::min<std::size_t>(7, static_cast<std::size_t>(point[0] * 8));
    const auto y =
        std::min<std::size_t>(7, static_cast<std::size_t>(point[1] * 8));
    ++bins[y * 8 + x];
  }
  const auto maximum = *std::max_element(bins.begin(), bins.end());
  ASC_RANDOM_TEST_CHECK(context, maximum < 32);

  std::array<std::uint64_t, 4> checksums{};
  std::array<std::thread, 4> threads;
  for (std::size_t thread = 0; thread < threads.size(); ++thread) {
    threads[thread] = std::thread([thread, &checksums] {
      std::uint64_t checksum = 1469598103934665603ULL;
      const auto prime = asc::PrimeAt(thread * 1000);
      if (!prime.ok()) {
        checksums[thread] = 0;
        return;
      }
      checksum = (checksum ^ *prime) * 1099511628211ULL;
      for (std::uint64_t index = 0; index < 1024; ++index) {
        const auto halton = asc::HaltonCoordinate<double>(index, thread);
        const auto value = asc::SobolWord(index, thread * 1000);
        if (!halton.ok() || !value.ok()) {
          checksums[thread] = 0;
          return;
        }
        checksum = (checksum ^ std::bit_cast<std::uint64_t>(*halton)) *
                   1099511628211ULL;
        checksum = (checksum ^ *value) * 1099511628211ULL;
      }
      checksums[thread] = checksum;
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (std::size_t thread = 0; thread < checksums.size(); ++thread) {
    std::uint64_t expected = 1469598103934665603ULL;
    const auto prime = asc::PrimeAt(thread * 1000);
    ASC_RANDOM_TEST_CHECK(context, prime.ok());
    expected = (expected ^ *prime) * 1099511628211ULL;
    for (std::uint64_t index = 0; index < 1024; ++index) {
      const auto halton = asc::HaltonCoordinate<double>(index, thread);
      const auto value = asc::SobolWord(index, thread * 1000);
      ASC_RANDOM_TEST_CHECK(context, halton.ok());
      ASC_RANDOM_TEST_CHECK(context, value.ok());
      expected =
          (expected ^ std::bit_cast<std::uint64_t>(*halton)) * 1099511628211ULL;
      expected = (expected ^ *value) * 1099511628211ULL;
    }
    ASC_RANDOM_TEST_EQ(context, checksums[thread], expected);
  }
}

void CheckNoSuccessfulPathAllocation(asc_random_test::TestContext& context) {
  asc::SplitMix64 engine(7);
  std::array<std::uint32_t, 16> workspace{};
  std::array<double, 16> latin{};
  std::array<double, 4> point{};
  std::size_t allocation_count = 0;
  bool succeeded = false;
  {
    asc_random_storage_benchmark::AllocationProbe probe;
    const auto radical = asc::RadicalInverse<double>(7, 123456);
    const asc::Status latin_status =
        asc::GenerateLatinHypercubeJittered<double>(4, 4, engine, workspace,
                                                    latin);
    const asc::Status halton_status =
        asc::GenerateHaltonPoint<double>(123, point);
    const asc::Status hammersley_status =
        asc::GenerateHammersleyPoint<double>(123, 1024, point);
    const asc::Status sobol_status =
        asc::GenerateSobolPoint<double>(123, point);
    succeeded = radical.ok() && latin_status.ok() && halton_status.ok() &&
                hammersley_status.ok() && sobol_status.ok();
    allocation_count = probe.count();
  }
  ASC_RANDOM_TEST_CHECK(context, succeeded);
  ASC_RANDOM_TEST_CHECK(
      context, asc_test::ProcessAllocationCountMatches(allocation_count, 0));
}

}  // namespace

int main() {
  asc_random_test::TestContext context;
  CheckPrimeAndRadicalInverse(context);
  CheckPermutationAndLatin(context);
  CheckHaltonAndHammersley(context);
  CheckSobol(context);
  CheckDiscrepancyAndConcurrency(context);
  CheckNoSuccessfulPathAllocation(context);
  return context.Finish();
}
