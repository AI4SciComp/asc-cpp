#ifndef ASC_RANDOM_QUASI_H_
#define ASC_RANDOM_QUASI_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/distribution.h"
#include "asc/random/export.h"

namespace asc {

inline constexpr std::size_t kSobolDimensionCount = 21201;
inline constexpr std::size_t kSobolDirectionWordCount = 64;
inline constexpr std::uint32_t kRandomQmcSequenceVersion1 = 1;

[[nodiscard]] ASC_RANDOM_EXPORT Result<std::uint32_t> PrimeAt(
    std::size_t index);

template <SupportedRandomReal Real>
[[nodiscard]] Result<Real> RadicalInverse(std::uint32_t base,
                                          std::uint64_t index) {
  if (base < 2) {
    return Status(ErrorCode::kInvalidArgument,
                  "Radical inverse base must be at least two");
  }
  const Real inverse_base = Real{1} / static_cast<Real>(base);
  Real factor = Real{1};
  Real value = Real{0};
  while (index != 0) {
    const auto digit = static_cast<std::uint32_t>(index % base);
    factor *= inverse_base;
    value = std::fma(static_cast<Real>(digit), factor, value);
    index /= base;
  }
  if (value == Real{1}) {
    value = std::nextafter(Real{1}, Real{0});
  }
  if (!std::isfinite(value) || value < Real{0} || !(value < Real{1})) {
    return Status(ErrorCode::kNumerical,
                  "Radical inverse produced an invalid result");
  }
  return value;
}

namespace internal_random_qmc {

inline Status ValidateDigitPermutation(
    std::uint32_t base, std::span<const std::uint32_t> permutation) {
  if (base < 2) {
    return Status(ErrorCode::kInvalidArgument,
                  "Scrambled radical inverse base must be at least two");
  }
  if (permutation.size() != static_cast<std::size_t>(base)) {
    return Status(ErrorCode::kShape,
                  "Digit permutation size must equal the base");
  }
  if (permutation[0] != 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Digit permutation must map zero to zero");
  }
  for (std::size_t index = 0; index < permutation.size(); ++index) {
    if (permutation[index] >= base) {
      return Status(ErrorCode::kInvalidArgument,
                    "Digit permutation contains an out-of-range value");
    }
    for (std::size_t other = 0; other < index; ++other) {
      if (permutation[other] == permutation[index]) {
        return Status(ErrorCode::kInvalidArgument,
                      "Digit permutation contains a repeated value");
      }
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
[[nodiscard]] Result<Real> RepairUnitInterval(Real value,
                                              const char* operation) {
  if (value == Real{1}) {
    value = std::nextafter(Real{1}, Real{0});
  }
  if (!std::isfinite(value) || value < Real{0} || !(value < Real{1})) {
    return Status(ErrorCode::kNumerical, operation);
  }
  return value;
}

inline Result<std::size_t> CheckedElementCount(std::size_t sample_count,
                                               std::size_t dimension_count) {
  if (dimension_count != 0 &&
      sample_count >
          std::numeric_limits<std::size_t>::max() / dimension_count) {
    return Status(ErrorCode::kOverflow, "QMC output element count overflows");
  }
  return sample_count * dimension_count;
}

template <typename Left, typename Right>
[[nodiscard]] bool SpansOverlap(std::span<Left> left,
                                std::span<Right> right) noexcept {
  if (left.empty() || right.empty()) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right.data());
  if (left_begin <= right_begin) {
    return right_begin - left_begin < left.size_bytes();
  }
  return left_begin - right_begin < right.size_bytes();
}

template <SupportedRandomReal Real>
[[nodiscard]] Real SobolUnit(std::uint64_t word) noexcept {
  if constexpr (std::same_as<Real, float>) {
    return static_cast<float>(word >> 40U) * 0x1.0p-24F;
  } else {
    return static_cast<double>(word >> 11U) * 0x1.0p-53;
  }
}

}  // namespace internal_random_qmc

template <SupportedRandomReal Real>
[[nodiscard]] Result<Real> ScrambledRadicalInverse(
    std::uint32_t base, std::uint64_t index,
    std::span<const std::uint32_t> permutation) {
  const Status validation =
      internal_random_qmc::ValidateDigitPermutation(base, permutation);
  if (!validation.ok()) {
    return validation;
  }
  const Real inverse_base = Real{1} / static_cast<Real>(base);
  Real factor = Real{1};
  Real value = Real{0};
  while (index != 0) {
    const auto digit = static_cast<std::uint32_t>(index % base);
    factor *= inverse_base;
    value = std::fma(static_cast<Real>(permutation[digit]), factor, value);
    index /= base;
  }
  return internal_random_qmc::RepairUnitInterval(
      value, "Scrambled radical inverse produced an invalid result");
}

template <CanonicalRandomEngine Engine>
Status GenerateLowDiscrepancyPermutation(Engine& engine,
                                         std::span<std::uint32_t> permutation) {
  if (permutation.size() >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Status(ErrorCode::kOverflow,
                  "Permutation size exceeds the uint32 domain");
  }
  for (std::size_t index = 0; index < permutation.size(); ++index) {
    permutation[index] = static_cast<std::uint32_t>(index);
  }
  for (std::size_t limit = permutation.size(); limit > 1; --limit) {
    auto distribution =
        UniformIntegerDistribution<std::size_t>::Create(0, limit - 1);
    const auto selected = (*distribution)(engine);
    std::swap(permutation[limit - 1], permutation[*selected]);
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status GenerateLatinHypercubeMidpoints(
    std::size_t sample_count, std::size_t dimension_count, Engine& engine,
    std::span<std::uint32_t> permutation_workspace, std::span<Real> output) {
  const auto count =
      internal_random_qmc::CheckedElementCount(sample_count, dimension_count);
  if (!count.ok()) {
    return count.status();
  }
  if (output.size() != *count || permutation_workspace.size() != *count) {
    return Status(ErrorCode::kShape,
                  "Latin output and workspace must match the logical shape");
  }
  if (*count == 0) {
    return Status::Ok();
  }
  if (sample_count == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Nonempty Latin output requires a positive sample count");
  }
  if (sample_count >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Status(ErrorCode::kOverflow,
                  "Latin sample count exceeds the uint32 domain");
  }
  if (internal_random_qmc::SpansOverlap(permutation_workspace, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Latin output and permutation workspace overlap");
  }

  for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
    const Status status = GenerateLowDiscrepancyPermutation(
        engine,
        permutation_workspace.subspan(dimension * sample_count, sample_count));
    if (!status.ok()) {
      return status;
    }
  }
  const Real denominator = static_cast<Real>(sample_count);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      const std::size_t workspace_index = dimension * sample_count + sample;
      const Real value =
          (static_cast<Real>(permutation_workspace[workspace_index]) +
           Real{0.5}) /
          denominator;
      auto repaired = internal_random_qmc::RepairUnitInterval(
          value, "Latin midpoint produced an invalid result");
      if (!repaired.ok()) {
        return repaired.status();
      }
      output[sample * dimension_count + dimension] = *repaired;
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status GenerateLatinHypercubeJittered(
    std::size_t sample_count, std::size_t dimension_count, Engine& engine,
    std::span<std::uint32_t> permutation_workspace, std::span<Real> output) {
  const auto count =
      internal_random_qmc::CheckedElementCount(sample_count, dimension_count);
  if (!count.ok()) {
    return count.status();
  }
  if (output.size() != *count || permutation_workspace.size() != *count) {
    return Status(ErrorCode::kShape,
                  "Latin output and workspace must match the logical shape");
  }
  if (*count == 0) {
    return Status::Ok();
  }
  if (sample_count == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Nonempty Latin output requires a positive sample count");
  }
  if (sample_count >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Status(ErrorCode::kOverflow,
                  "Latin sample count exceeds the uint32 domain");
  }
  if (internal_random_qmc::SpansOverlap(permutation_workspace, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Latin output and permutation workspace overlap");
  }

  for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
    const Status status = GenerateLowDiscrepancyPermutation(
        engine,
        permutation_workspace.subspan(dimension * sample_count, sample_count));
    if (!status.ok()) {
      return status;
    }
  }
  const Real denominator = static_cast<Real>(sample_count);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      const std::size_t workspace_index = dimension * sample_count + sample;
      const Real unit = internal_random_distribution::UnitReal<Real>(engine);
      Real value =
          (static_cast<Real>(permutation_workspace[workspace_index]) + unit) /
          denominator;
      const Real stratum_upper =
          (static_cast<Real>(permutation_workspace[workspace_index]) +
           Real{1}) /
          denominator;
      if (value == stratum_upper) {
        value = std::nextafter(stratum_upper, Real{0});
      }
      auto repaired = internal_random_qmc::RepairUnitInterval(
          value, "Latin jitter produced an invalid result");
      if (!repaired.ok()) {
        return repaired.status();
      }
      output[sample * dimension_count + dimension] = *repaired;
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
[[nodiscard]] Result<Real> HaltonCoordinate(std::uint64_t index,
                                            std::size_t dimension) {
  const auto prime = PrimeAt(dimension);
  if (!prime.ok()) {
    return prime.status();
  }
  return RadicalInverse<Real>(*prime, index);
}

template <SupportedRandomReal Real>
[[nodiscard]] Result<Real> ScrambledHaltonCoordinate(
    std::uint64_t index, std::size_t dimension,
    std::span<const std::uint32_t> permutation) {
  const auto prime = PrimeAt(dimension);
  if (!prime.ok()) {
    return prime.status();
  }
  return ScrambledRadicalInverse<Real>(*prime, index, permutation);
}

template <SupportedRandomReal Real>
Status GenerateHaltonPoint(std::uint64_t index, std::span<Real> output) {
  if (output.size() > kSobolDimensionCount) {
    return Status(ErrorCode::kShape,
                  "Halton dimension exceeds the supported prime domain");
  }
  for (std::size_t dimension = 0; dimension < output.size(); ++dimension) {
    const auto value = HaltonCoordinate<Real>(index, dimension);
    if (!value.ok()) {
      return value.status();
    }
    output[dimension] = *value;
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status GenerateScrambledHaltonPoint(
    std::uint64_t index,
    std::span<const std::span<const std::uint32_t>> permutations,
    std::span<Real> output) {
  if (permutations.size() != output.size()) {
    return Status(ErrorCode::kShape,
                  "Halton digit permutations must match the point dimension");
  }
  if (output.size() > kSobolDimensionCount) {
    return Status(ErrorCode::kShape,
                  "Halton dimension exceeds the supported prime domain");
  }
  for (std::size_t dimension = 0; dimension < output.size(); ++dimension) {
    const auto prime = PrimeAt(dimension);
    if (!prime.ok()) {
      return prime.status();
    }
    const Status validation = internal_random_qmc::ValidateDigitPermutation(
        *prime, permutations[dimension]);
    if (!validation.ok()) {
      return validation;
    }
    if (internal_random_qmc::SpansOverlap(permutations[dimension], output)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Halton output and digit permutation overlap");
    }
  }
  for (std::size_t dimension = 0; dimension < output.size(); ++dimension) {
    const auto value = ScrambledHaltonCoordinate<Real>(index, dimension,
                                                       permutations[dimension]);
    if (!value.ok()) {
      return value.status();
    }
    output[dimension] = *value;
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status GenerateHammersleyPoint(std::uint64_t index, std::uint64_t total_count,
                               std::span<Real> output) {
  if (total_count == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Hammersley total count must be positive");
  }
  if (index >= total_count) {
    return Status(ErrorCode::kIndex,
                  "Hammersley index must be below the total count");
  }
  if (output.size() > kSobolDimensionCount + 1) {
    return Status(ErrorCode::kShape,
                  "Hammersley dimension exceeds the supported prime domain");
  }
  if (output.empty()) {
    return Status::Ok();
  }
  auto first = internal_random_qmc::RepairUnitInterval(
      static_cast<Real>(index) / static_cast<Real>(total_count),
      "Hammersley leading coordinate produced an invalid result");
  if (!first.ok()) {
    return first.status();
  }
  output[0] = *first;
  for (std::size_t dimension = 1; dimension < output.size(); ++dimension) {
    const auto prime = PrimeAt(dimension - 1);
    if (!prime.ok()) {
      return prime.status();
    }
    const auto value = RadicalInverse<Real>(*prime, index);
    if (!value.ok()) {
      return value.status();
    }
    output[dimension] = *value;
  }
  return Status::Ok();
}

ASC_RANDOM_EXPORT Status InitializeSobolDirectionNumbers(
    std::size_t dimension, std::span<std::uint64_t> direction_words);

[[nodiscard]] ASC_RANDOM_EXPORT std::uint64_t
SobolDirectionTableChecksum() noexcept;

[[nodiscard]] ASC_RANDOM_EXPORT Result<std::uint64_t> SobolWord(
    std::uint64_t index, std::size_t dimension);

template <SupportedRandomReal Real>
[[nodiscard]] Result<Real> SobolCoordinate(std::uint64_t index,
                                           std::size_t dimension) {
  const auto word = SobolWord(index, dimension);
  if (!word.ok()) {
    return word.status();
  }
  return internal_random_qmc::SobolUnit<Real>(*word);
}

template <SupportedRandomReal Real>
Status GenerateSobolPoint(std::uint64_t index, std::span<Real> output) {
  if (output.size() > kSobolDimensionCount) {
    return Status(ErrorCode::kShape,
                  "Sobol dimension exceeds the Joe-Kuo table");
  }
  for (std::size_t dimension = 0; dimension < output.size(); ++dimension) {
    const auto value = SobolCoordinate<Real>(index, dimension);
    if (!value.ok()) {
      return value.status();
    }
    output[dimension] = *value;
  }
  return Status::Ok();
}

class ASC_RANDOM_EXPORT SobolSequence final {
 public:
  [[nodiscard]] static Result<SobolSequence> Create(
      std::size_t dimension_count, std::uint64_t initial_index = 0);

  template <SupportedRandomReal Real>
  Status Next(std::span<Real> output) {
    if (output.size() != dimension_count_) {
      return Status(ErrorCode::kShape,
                    "Sobol output size differs from the sequence dimension");
    }
    if (output.empty()) {
      return Status::Ok();
    }
    if (exhausted_) {
      return Status(ErrorCode::kEndOfFile,
                    "Sobol sequence exhausted the uint64 index domain");
    }
    const Status status = GenerateSobolPoint(index_, output);
    if (!status.ok()) {
      return status;
    }
    if (index_ == std::numeric_limits<std::uint64_t>::max()) {
      exhausted_ = true;
    } else {
      ++index_;
    }
    return Status::Ok();
  }

  Status Reset(std::uint64_t index = 0);
  Status Skip(std::uint64_t count);
  Status SkipTo(std::uint64_t index);

  [[nodiscard]] std::size_t dimension_count() const noexcept {
    return dimension_count_;
  }
  [[nodiscard]] std::uint64_t index() const noexcept { return index_; }
  [[nodiscard]] bool exhausted() const noexcept { return exhausted_; }

 private:
  SobolSequence(std::size_t dimension_count, std::uint64_t initial_index)
      : dimension_count_(dimension_count), index_(initial_index) {}

  std::size_t dimension_count_;
  std::uint64_t index_;
  bool exhausted_ = false;
};

}  // namespace asc

#endif  // ASC_RANDOM_QUASI_H_
