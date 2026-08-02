#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <utility>

#include "allocation_probe.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/quasi.h"

namespace {

constexpr std::size_t kEngineRepetitions = 200000;
constexpr std::size_t kDistributionRepetitions = 200000;
constexpr std::size_t kNormalRepetitions = 100000;
constexpr std::size_t kQmcRepetitions = 100000;

std::uint64_t Mix(std::uint64_t checksum, std::uint64_t value) {
  return (checksum ^ value) * 1099511628211ULL;
}

template <typename Engine>
bool BenchmarkEngine(std::string_view name, Engine engine,
                     std::uint64_t expected_checksum,
                     std::uint64_t& aggregate_checksum) {
  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t allocation_calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  {
    asc_random_storage_benchmark::AllocationProbe probe;
    for (std::size_t index = 0; index < kEngineRepetitions; ++index) {
      checksum = Mix(checksum, static_cast<std::uint64_t>(engine()));
    }
    allocation_calls = probe.count();
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  if (checksum != expected_checksum ||
      !asc_test::ProcessAllocationCountMatches(allocation_calls, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "operation=raw-engine algorithm=" << name
            << " sequence_version=1 repetitions=" << kEngineRepetitions
            << " allocation_calls=" << allocation_calls
            << " elapsed_ns=" << elapsed.count() << " checksum=" << checksum
            << " correctness=known-answer-derived-checksum\n";
  return true;
}

bool BenchmarkUniformInteger(std::uint64_t& aggregate_checksum) {
  auto distribution =
      asc::UniformIntegerDistribution<std::int32_t>::Create(-1000, 1000);
  if (!distribution.ok()) {
    return false;
  }
  asc::Pcg32 engine(123, 456);
  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t allocation_calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  {
    asc_random_storage_benchmark::AllocationProbe probe;
    for (std::size_t index = 0; index < kDistributionRepetitions; ++index) {
      const auto value = (*distribution)(engine);
      if (!value.ok() || *value < -1000 || *value > 1000) {
        return false;
      }
      checksum = Mix(checksum, static_cast<std::uint64_t>(*value));
    }
    allocation_calls = probe.count();
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  constexpr std::uint64_t kExpectedChecksum = 0xC7B222FE03542292ULL;
  if (checksum != kExpectedChecksum ||
      !asc_test::ProcessAllocationCountMatches(allocation_calls, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "operation=uniform-integer algorithm=PCG32-v1"
            << " seed=123 stream=456 interval=[-1000,1000]"
            << " repetitions=" << kDistributionRepetitions
            << " allocation_calls=" << allocation_calls
            << " elapsed_ns=" << elapsed.count() << " checksum=" << checksum
            << " correctness=independent-sequence-checksum\n";
  return true;
}

bool BenchmarkUniformReal(std::uint64_t& aggregate_checksum) {
  auto distribution = asc::UniformRealDistribution<double>::Create(0, 1);
  if (!distribution.ok()) {
    return false;
  }
  asc::Xoroshiro128Plus engine(987);
  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t allocation_calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  {
    asc_random_storage_benchmark::AllocationProbe probe;
    for (std::size_t index = 0; index < kDistributionRepetitions; ++index) {
      const auto value = (*distribution)(engine);
      if (!value.ok() || *value < 0 || !(*value < 1)) {
        return false;
      }
      checksum = Mix(checksum, std::bit_cast<std::uint64_t>(*value));
    }
    allocation_calls = probe.count();
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  constexpr std::uint64_t kExpectedChecksum = 0x9EB2F1CFAD9F6CA8ULL;
  if (checksum != kExpectedChecksum ||
      !asc_test::ProcessAllocationCountMatches(allocation_calls, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "operation=uniform-real algorithm=xoroshiro128plus-v1"
            << " seed=987 scalar=double interval=[0,1)"
            << " repetitions=" << kDistributionRepetitions
            << " allocation_calls=" << allocation_calls
            << " elapsed_ns=" << elapsed.count() << " checksum=" << checksum
            << " correctness=independent-sequence-checksum\n";
  return true;
}

bool BenchmarkNormal(std::uint64_t& aggregate_checksum) {
  auto distribution = asc::NormalDistribution<double>::Create(0, 1);
  if (!distribution.ok()) {
    return false;
  }
  asc::SplitMix64 engine(2024);
  long double sum = 0;
  long double squared_sum = 0;
  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t allocation_calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  {
    asc_random_storage_benchmark::AllocationProbe probe;
    for (std::size_t index = 0; index < kNormalRepetitions; ++index) {
      const auto value = (*distribution)(engine);
      if (!value.ok()) {
        return false;
      }
      sum += *value;
      squared_sum += static_cast<long double>(*value) * *value;
      checksum = Mix(checksum, std::bit_cast<std::uint64_t>(*value));
    }
    allocation_calls = probe.count();
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  const long double mean = sum / kNormalRepetitions;
  const long double variance = squared_sum / kNormalRepetitions - mean * mean;
  if (std::abs(mean) > 0.03L || std::abs(variance - 1) > 0.05L ||
      !asc_test::ProcessAllocationCountMatches(allocation_calls, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "operation=normal algorithm=box-muller-v1"
            << " engine=SplitMix64-v1 seed=2024 scalar=double mean=0 sd=1"
            << " repetitions=" << kNormalRepetitions
            << " allocation_calls=" << allocation_calls
            << " elapsed_ns=" << elapsed.count() << " checksum=" << checksum
            << " observed_mean=" << static_cast<double>(mean)
            << " observed_variance=" << static_cast<double>(variance)
            << " correctness=fixed-seed-moment-invariant\n";
  return true;
}

bool BenchmarkSobol(std::uint64_t& aggregate_checksum) {
  std::array<double, 3> published{};
  if (!asc::GenerateSobolPoint<double>(4, published).ok() ||
      published != std::array<double, 3>{0.375, 0.375, 0.625}) {
    return false;
  }
  std::uint64_t checksum = 1469598103934665603ULL;
  std::size_t allocation_calls = 0;
  const auto begin = std::chrono::steady_clock::now();
  {
    asc_random_storage_benchmark::AllocationProbe probe;
    for (std::size_t index = 0; index < kQmcRepetitions; ++index) {
      const auto word = asc::SobolWord(index, 3);
      if (!word.ok()) {
        return false;
      }
      checksum = Mix(checksum, *word);
    }
    allocation_calls = probe.count();
  }
  const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - begin);
  constexpr std::uint64_t kExpectedChecksum = 0xB9C9B02511675303ULL;
  if (checksum != kExpectedChecksum ||
      !asc_test::ProcessAllocationCountMatches(allocation_calls, 0)) {
    return false;
  }
  aggregate_checksum = Mix(aggregate_checksum, checksum);
  std::cout << "operation=qmc-indexed-word algorithm=Sobol-Joe-Kuo-D6-v1"
            << " dimension=3 repetitions=" << kQmcRepetitions
            << " allocation_calls=" << allocation_calls
            << " elapsed_ns=" << elapsed.count() << " checksum=" << checksum
            << " correctness=published-point-and-independent-checksum\n";
  return true;
}

}  // namespace

int main() {
#ifdef NDEBUG
  constexpr std::string_view kConfiguration = "release";
#else
  constexpr std::string_view kConfiguration = "debug";
#endif
#if defined(_MSC_FULL_VER)
  std::cout << "compiler=MSVC-" << _MSC_FULL_VER;
#elif defined(__clang_version__)
  std::cout << "compiler=Clang-" << __clang_version__;
#elif defined(__VERSION__)
  std::cout << "compiler=" << __VERSION__;
#else
  std::cout << "compiler=unknown";
#endif
  std::cout << " configuration=" << kConfiguration << '\n';

  std::uint64_t checksum = 1469598103934665603ULL;
  if (!BenchmarkEngine("SplitMix64-v1", asc::SplitMix64(0),
                       0x31094AC056EF6F0AULL, checksum) ||
      !BenchmarkEngine("PCG32-v1", asc::Pcg32(42, 54), 0xB74EA408B377181AULL,
                       checksum) ||
      !BenchmarkEngine("xoroshiro64star-v1", asc::Xoroshiro64Star(0),
                       0xB47E791CF8607C35ULL, checksum) ||
      !BenchmarkEngine("xoroshiro128plus-v1", asc::Xoroshiro128Plus(0),
                       0x9B8AB4AC4D3E0E4CULL, checksum) ||
      !BenchmarkUniformInteger(checksum) || !BenchmarkUniformReal(checksum) ||
      !BenchmarkNormal(checksum) || !BenchmarkSobol(checksum)) {
    return 1;
  }
  std::cout << "aggregate_checksum=" << checksum << '\n';
  return 0;
}
