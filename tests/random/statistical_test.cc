#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "asc/random/distribution.h"
#include "asc/random/engine.h"

namespace {

constexpr std::size_t kUniformSamples = 65536;
constexpr std::size_t kUniformBins = 16;
constexpr double kMaximumChiSquare = 72.0;
constexpr std::size_t kNormalSamples = 131072;
constexpr double kMaximumMeanMagnitude = 0.02;
constexpr double kMaximumVarianceError = 0.03;

double ChiSquare(const std::array<std::size_t, kUniformBins>& bins) {
  constexpr double kExpected =
      static_cast<double>(kUniformSamples) / kUniformBins;
  double statistic = 0;
  for (const std::size_t observed : bins) {
    const double difference = static_cast<double>(observed) - kExpected;
    statistic += difference * difference / kExpected;
  }
  return statistic;
}

bool CheckUniformInteger() {
  auto distribution =
      asc::UniformIntegerDistribution<std::uint32_t>::Create(0, 15);
  if (!distribution.ok()) {
    return false;
  }
  asc::Pcg32 engine(0x0123456789ABCDEFULL, 0x13579BDF2468ACE0ULL);
  std::array<std::size_t, kUniformBins> bins{};
  for (std::size_t index = 0; index < kUniformSamples; ++index) {
    const auto value = (*distribution)(engine);
    if (!value.ok()) {
      return false;
    }
    ++bins[*value];
  }
  const double statistic = ChiSquare(bins);
  if (statistic > kMaximumChiSquare) {
    std::cerr << "uniform-integer statistical check failed: engine=PCG32-v1"
              << " seed=0x0123456789abcdef stream=0x13579bdf2468ace0"
              << " samples=" << kUniformSamples << " bins=" << kUniformBins
              << " chi_square=" << statistic
              << " threshold=" << kMaximumChiSquare << '\n';
    return false;
  }
  return true;
}

bool CheckUniformReal() {
  auto distribution = asc::UniformRealDistribution<double>::Create(0, 1);
  if (!distribution.ok()) {
    return false;
  }
  asc::Xoroshiro128Plus engine(0xFEDCBA9876543210ULL);
  std::array<std::size_t, kUniformBins> bins{};
  for (std::size_t index = 0; index < kUniformSamples; ++index) {
    const auto value = (*distribution)(engine);
    if (!value.ok() || *value < 0 || !(*value < 1)) {
      return false;
    }
    const std::size_t bin =
        static_cast<std::size_t>(*value * static_cast<double>(kUniformBins));
    ++bins[bin];
  }
  const double statistic = ChiSquare(bins);
  if (statistic > kMaximumChiSquare) {
    std::cerr << "uniform-real statistical check failed: "
              << "engine=xoroshiro128plus-v1 seed=0xfedcba9876543210"
              << " samples=" << kUniformSamples << " bins=" << kUniformBins
              << " chi_square=" << statistic
              << " threshold=" << kMaximumChiSquare << '\n';
    return false;
  }
  return true;
}

bool CheckNormal() {
  auto distribution = asc::NormalDistribution<double>::Create(0, 1);
  if (!distribution.ok()) {
    return false;
  }
  asc::SplitMix64 engine(0x243F6A8885A308D3ULL);
  long double sum = 0;
  long double squared_sum = 0;
  for (std::size_t index = 0; index < kNormalSamples; ++index) {
    const auto value = (*distribution)(engine);
    if (!value.ok()) {
      return false;
    }
    sum += *value;
    squared_sum += static_cast<long double>(*value) * *value;
  }
  const long double mean = sum / kNormalSamples;
  const long double variance = squared_sum / kNormalSamples - mean * mean;
  const long double mean_magnitude = std::abs(mean);
  const long double variance_error = std::abs(variance - 1);
  if (mean_magnitude > kMaximumMeanMagnitude ||
      variance_error > kMaximumVarianceError) {
    std::cerr << "normal statistical check failed: engine=SplitMix64-v1"
              << " seed=0x243f6a8885a308d3 samples=" << kNormalSamples
              << " mean=" << static_cast<double>(mean)
              << " mean_threshold=" << kMaximumMeanMagnitude
              << " variance=" << static_cast<double>(variance)
              << " variance_error_threshold=" << kMaximumVarianceError << '\n';
    return false;
  }
  return true;
}

}  // namespace

int main() {
  return CheckUniformInteger() && CheckUniformReal() && CheckNormal() ? 0 : 1;
}
