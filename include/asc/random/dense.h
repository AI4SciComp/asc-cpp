#ifndef ASC_RANDOM_DENSE_H_
#define ASC_RANDOM_DENSE_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/generator.h"
#include "asc/random/quasi.h"

namespace asc {

inline constexpr std::uint32_t kRandomDenseSamplerSequenceVersion1 = 1;
inline constexpr std::size_t kUnitSphereMaximumAttemptsVersion1 = 64;

namespace internal_random_dense {

template <typename Element>
constexpr std::uint64_t WordsPerElement() noexcept {
  if constexpr (std::same_as<Element, float>) {
    return 1;
  } else {
    return 2;
  }
}

template <typename Element>
Element GenerateUniform01(RandomStream stream, RandomSubsequence subsequence,
                          RandomOffset offset) noexcept {
  if constexpr (std::same_as<Element, float>) {
    return Uniform01<float>(
        GeneratePhilox4x32Word(stream, subsequence, offset));
  } else {
    return Uniform01<double>(
        GeneratePhilox4x32Word(stream, subsequence, offset),
        GeneratePhilox4x32Word(stream, subsequence, offset + 1));
  }
}

template <typename Generator, typename Element>
concept ValueGeneratorFor = requires(Generator& generator) {
  { generator() } -> std::same_as<Result<Element>>;
};

template <typename Element, std::size_t Rank>
Status ValidateSerialHost(const ExecutionContext& context,
                          const DenseView<Element, Rank>& view,
                          const char* operation) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported, operation);
  }
  if (view.memory_space() != MemorySpace::kHost ||
      !context.CanAccess(view.memory_space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Dense random operation requires host storage");
  }
  return Status::Ok();
}

template <typename Element, std::size_t Rank>
Element& ElementAt(DenseView<Element, Rank> view,
                   std::span<const std::size_t, Rank> coordinates) noexcept {
  std::size_t offset = 0;
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    offset += coordinates[dimension] *
              static_cast<std::size_t>(view.strides()[dimension]);
  }
  return view.data()[offset];
}

template <typename Element, std::size_t Rank>
Element& LogicalElement(DenseView<Element, Rank> view,
                        std::uint64_t ordinal) noexcept {
  std::size_t physical_offset = 0;
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    const auto extent = static_cast<std::uint64_t>(view.extents()[dimension]);
    const std::uint64_t coordinate = ordinal % extent;
    ordinal /= extent;
    physical_offset += static_cast<std::size_t>(coordinate) *
                       static_cast<std::size_t>(view.strides()[dimension]);
  }
  return view.data()[physical_offset];
}

inline bool ByteRangesOverlap(const void* left, std::size_t left_size,
                              const void* right,
                              std::size_t right_size) noexcept {
  if (left_size == 0 || right_size == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left);
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right);
  if (left_size > UINTPTR_MAX - left_begin ||
      right_size > UINTPTR_MAX - right_begin) {
    return true;
  }
  return left_begin < right_begin + right_size &&
         right_begin < left_begin + left_size;
}

template <typename Left, std::size_t LeftRank, typename Right,
          std::size_t RightRank>
bool DenseViewsOverlap(const DenseView<Left, LeftRank>& left,
                       const DenseView<Right, RightRank>& right) noexcept {
  return ByteRangesOverlap(
      left.data(), left.mapping().required_span_size() * sizeof(Left),
      right.data(), right.mapping().required_span_size() * sizeof(Right));
}

template <typename Element, std::size_t Rank, typename SpanElement>
bool DenseViewOverlapsSpan(const DenseView<Element, Rank>& view,
                           std::span<SpanElement> values) noexcept {
  return ByteRangesOverlap(
      view.data(), view.mapping().required_span_size() * sizeof(Element),
      values.data(), values.size_bytes());
}

template <SupportedRandomReal Real>
Status ValidateQmcDestination(const ExecutionContext& context,
                              DenseView<Real, 2> destination,
                              DenseView<Real, 1> point_workspace,
                              std::size_t maximum_dimension,
                              const char* operation) {
  Status status = ValidateSerialHost(context, destination, operation);
  if (!status.ok()) {
    return status;
  }
  status = ValidateSerialHost(context, point_workspace, operation);
  if (!status.ok()) {
    return status;
  }
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  if (dimension_count > maximum_dimension) {
    return Status(ErrorCode::kShape,
                  "Dense QMC dimension exceeds the supported domain");
  }
  if (point_workspace.extents()[0] != destination.extents()[1]) {
    return Status(ErrorCode::kShape,
                  "Dense QMC point workspace has the wrong dimension");
  }
  if (DenseViewsOverlap(destination, point_workspace)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Dense QMC destination and workspace overlap");
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status ValidateQmcIndexRange(DenseView<Real, 2> destination,
                             std::uint64_t initial_index) {
  auto sample_count = CheckedCast<std::uint64_t>(destination.extents()[0]);
  if (!sample_count.ok()) {
    return sample_count.status();
  }
  if (*sample_count == 0) {
    return Status::Ok();
  }
  auto final_index =
      CheckedAdd<std::uint64_t>(initial_index, *sample_count - 1);
  if (!final_index.ok()) {
    return Status(ErrorCode::kOverflow, "Dense QMC index range overflows");
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status PrepareLatinHypercube(
    const ExecutionContext& context, DenseView<Real, 2> destination,
    Engine& engine, DenseView<std::uint32_t, 2> permutation_workspace) {
  Status status = ValidateSerialHost(
      context, destination,
      "Dense Latin hypercube fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = ValidateSerialHost(
      context, permutation_workspace,
      "Dense Latin hypercube fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  if (destination.extents()[0] != permutation_workspace.extents()[0] ||
      destination.extents()[1] != permutation_workspace.extents()[1]) {
    return Status(ErrorCode::kShape,
                  "Dense Latin permutation workspace shape differs");
  }
  if (DenseViewsOverlap(destination, permutation_workspace)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Dense Latin destination and workspace overlap");
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  if (sample_count >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Status(ErrorCode::kOverflow,
                  "Dense Latin sample count exceeds the uint32 domain");
  }

  for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
      const std::array<std::size_t, 2> coordinate{sample, dimension};
      ElementAt(permutation_workspace,
                std::span<const std::size_t, 2>(coordinate)) =
          static_cast<std::uint32_t>(sample);
    }
    for (std::size_t limit = sample_count; limit > 1; --limit) {
      auto selection =
          UniformIntegerDistribution<std::size_t>::Create(0, limit - 1);
      const auto selected = (*selection)(engine);
      const std::array<std::size_t, 2> left{limit - 1, dimension};
      const std::array<std::size_t, 2> right{*selected, dimension};
      std::swap(ElementAt(permutation_workspace,
                          std::span<const std::size_t, 2>(left)),
                ElementAt(permutation_workspace,
                          std::span<const std::size_t, 2>(right)));
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
void PublishPoint(DenseView<Real, 2> destination, std::size_t sample,
                  DenseView<Real, 1> point_workspace) noexcept {
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
    const std::array<std::size_t, 2> output_coordinate{sample, dimension};
    const std::array<std::size_t, 1> workspace_coordinate{dimension};
    ElementAt(destination, std::span<const std::size_t, 2>(output_coordinate)) =
        ElementAt(point_workspace,
                  std::span<const std::size_t, 1>(workspace_coordinate));
  }
}

}  // namespace internal_random_dense

template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<RandomOffset> FillDenseUniform01(
    const ExecutionContext& context, DenseView<Element, Rank> destination,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset) {
  Status status = internal_random_dense::ValidateSerialHost(
      context, destination, "Dense random fill requires serial execution");
  if (!status.ok()) {
    return status;
  }

  auto logical_size = CheckedCast<std::uint64_t>(destination.logical_size());
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  constexpr std::uint64_t kWordsPerElement =
      internal_random_dense::WordsPerElement<Element>();
  auto word_count =
      CheckedMultiply<std::uint64_t>(*logical_size, kWordsPerElement);
  if (!word_count.ok()) {
    return word_count.status();
  }
  auto next_offset = AdvanceRandomOffset(offset, *word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }

  for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
    const RandomOffset word_offset = offset + ordinal * kWordsPerElement;
    internal_random_dense::LogicalElement(destination, ordinal) =
        internal_random_dense::GenerateUniform01<Element>(stream, subsequence,
                                                          word_offset);
  }
  return *next_offset;
}

template <typename Element, std::size_t Rank, typename Generator>
  requires internal_random_dense::ValueGeneratorFor<Generator, Element>
Status FillDensePseudo(const ExecutionContext& context,
                       DenseView<Element, Rank> destination,
                       Generator& generator) {
  Status status = internal_random_dense::ValidateSerialHost(
      context, destination, "Dense pseudo fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  auto logical_size = CheckedCast<std::uint64_t>(destination.logical_size());
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
    auto generated = generator();
    if (!generated.ok()) {
      return generated.status();
    }
    internal_random_dense::LogicalElement(destination, ordinal) = *generated;
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status PrepareDenseMultivariateNormal(
    const ExecutionContext& context,
    DenseView<const std::type_identity_t<Real>, 1> mean,
    DenseView<const std::type_identity_t<Real>, 2> covariance,
    DenseView<Real, 2> lower_factor) {
  Status status = internal_random_dense::ValidateSerialHost(
      context, lower_factor,
      "Multivariate normal preparation requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = internal_random_dense::ValidateSerialHost(
      context, mean,
      "Multivariate normal preparation requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = internal_random_dense::ValidateSerialHost(
      context, covariance,
      "Multivariate normal preparation requires serial execution");
  if (!status.ok()) {
    return status;
  }

  const extent_t dimension = mean.extents()[0];
  if (covariance.extents()[0] != dimension ||
      covariance.extents()[1] != dimension ||
      lower_factor.extents()[0] != dimension ||
      lower_factor.extents()[1] != dimension) {
    return Status(ErrorCode::kShape,
                  "Multivariate normal mean, covariance, and factor shapes "
                  "differ");
  }
  if (internal_random_dense::DenseViewsOverlap(lower_factor, mean) ||
      internal_random_dense::DenseViewsOverlap(lower_factor, covariance)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Multivariate normal factor overlaps an input");
  }

  const auto count = static_cast<std::size_t>(dimension);
  for (std::size_t row = 0; row < count; ++row) {
    const std::array<std::size_t, 1> mean_coordinate{row};
    if (!std::isfinite(internal_random_dense::ElementAt(
            mean, std::span<const std::size_t, 1>(mean_coordinate)))) {
      return Status(ErrorCode::kInvalidArgument,
                    "Multivariate normal mean must be finite");
    }
    for (std::size_t column = 0; column < count; ++column) {
      const std::array<std::size_t, 2> coordinate{row, column};
      const Real value = internal_random_dense::ElementAt(
          covariance, std::span<const std::size_t, 2>(coordinate));
      if (!std::isfinite(value)) {
        return Status(ErrorCode::kInvalidArgument,
                      "Multivariate normal covariance must be finite");
      }
      const std::array<std::size_t, 2> transpose{column, row};
      if (value !=
          internal_random_dense::ElementAt(
              covariance, std::span<const std::size_t, 2>(transpose))) {
        return Status(ErrorCode::kInvalidArgument,
                      "Multivariate normal covariance must be exactly "
                      "symmetric");
      }
    }
  }

  for (std::size_t row = 0; row < count; ++row) {
    for (std::size_t column = 0; column <= row; ++column) {
      const std::array<std::size_t, 2> coordinate{row, column};
      Real value = internal_random_dense::ElementAt(
          covariance, std::span<const std::size_t, 2>(coordinate));
      for (std::size_t inner = 0; inner < column; ++inner) {
        const std::array<std::size_t, 2> left_coordinate{row, inner};
        const std::array<std::size_t, 2> right_coordinate{column, inner};
        value = std::fma(
            -internal_random_dense::ElementAt(
                lower_factor, std::span<const std::size_t, 2>(left_coordinate)),
            internal_random_dense::ElementAt(
                lower_factor,
                std::span<const std::size_t, 2>(right_coordinate)),
            value);
      }
      if (row == column) {
        if (!std::isfinite(value) || !(value > Real{0})) {
          return Status(ErrorCode::kNumerical,
                        "Multivariate normal covariance is not positive "
                        "definite");
        }
        value = std::sqrt(value);
      } else {
        const std::array<std::size_t, 2> diagonal{column, column};
        value /= internal_random_dense::ElementAt(
            lower_factor, std::span<const std::size_t, 2>(diagonal));
      }
      if (!std::isfinite(value)) {
        return Status(ErrorCode::kNumerical,
                      "Multivariate normal factorization is nonfinite");
      }
      internal_random_dense::ElementAt(
          lower_factor, std::span<const std::size_t, 2>(coordinate)) = value;
    }
  }
  for (std::size_t row = 0; row < count; ++row) {
    for (std::size_t column = row + 1; column < count; ++column) {
      const std::array<std::size_t, 2> coordinate{row, column};
      internal_random_dense::ElementAt(
          lower_factor, std::span<const std::size_t, 2>(coordinate)) = Real{0};
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status FillDenseMultivariateNormal(
    const ExecutionContext& context, DenseView<Real, 2> destination,
    DenseView<const std::type_identity_t<Real>, 1> mean,
    DenseView<const std::type_identity_t<Real>, 2> lower_factor,
    NormalGenerator<Engine, Real>& standard_normal,
    DenseView<Real, 1> sample_workspace) {
  Status status = internal_random_dense::ValidateSerialHost(
      context, destination,
      "Multivariate normal sampling requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = internal_random_dense::ValidateSerialHost(
      context, mean, "Multivariate normal sampling requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = internal_random_dense::ValidateSerialHost(
      context, lower_factor,
      "Multivariate normal sampling requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = internal_random_dense::ValidateSerialHost(
      context, sample_workspace,
      "Multivariate normal sampling requires serial execution");
  if (!status.ok()) {
    return status;
  }

  const extent_t dimension = mean.extents()[0];
  if (destination.extents()[1] != dimension ||
      lower_factor.extents()[0] != dimension ||
      lower_factor.extents()[1] != dimension ||
      sample_workspace.extents()[0] != dimension) {
    return Status(ErrorCode::kShape,
                  "Multivariate normal sampling shapes differ");
  }
  if (internal_random_dense::DenseViewsOverlap(destination, mean) ||
      internal_random_dense::DenseViewsOverlap(destination, lower_factor) ||
      internal_random_dense::DenseViewsOverlap(destination, sample_workspace) ||
      internal_random_dense::DenseViewsOverlap(sample_workspace, mean) ||
      internal_random_dense::DenseViewsOverlap(sample_workspace,
                                               lower_factor)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Multivariate normal destination or workspace overlaps "
                  "read-only inputs");
  }
  if (standard_normal.distribution().mean() != Real{0} ||
      standard_normal.distribution().standard_deviation() != Real{1}) {
    return Status(ErrorCode::kInvalidArgument,
                  "Multivariate normal sampling requires a standard normal "
                  "generator");
  }

  const auto count = static_cast<std::size_t>(dimension);
  for (std::size_t row = 0; row < count; ++row) {
    const std::array<std::size_t, 1> mean_coordinate{row};
    if (!std::isfinite(internal_random_dense::ElementAt(
            mean, std::span<const std::size_t, 1>(mean_coordinate)))) {
      return Status(ErrorCode::kInvalidArgument,
                    "Multivariate normal mean must be finite");
    }
    for (std::size_t column = 0; column < count; ++column) {
      const std::array<std::size_t, 2> coordinate{row, column};
      const Real value = internal_random_dense::ElementAt(
          lower_factor, std::span<const std::size_t, 2>(coordinate));
      if (!std::isfinite(value) || (column > row && value != Real{0}) ||
          (column == row && !(value > Real{0}))) {
        return Status(ErrorCode::kInvalidArgument,
                      "Multivariate normal factor is not finite lower "
                      "triangular with a positive diagonal");
      }
    }
  }

  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    for (std::size_t dimension_index = 0; dimension_index < count;
         ++dimension_index) {
      auto generated = standard_normal();
      if (!generated.ok()) {
        return generated.status();
      }
      const std::array<std::size_t, 1> coordinate{dimension_index};
      internal_random_dense::ElementAt(
          sample_workspace, std::span<const std::size_t, 1>(coordinate)) =
          *generated;
    }
    for (std::size_t reverse = count; reverse > 0; --reverse) {
      const std::size_t row = reverse - 1;
      const std::array<std::size_t, 1> mean_coordinate{row};
      Real value = internal_random_dense::ElementAt(
          mean, std::span<const std::size_t, 1>(mean_coordinate));
      for (std::size_t column = 0; column <= row; ++column) {
        const std::array<std::size_t, 2> factor_coordinate{row, column};
        const std::array<std::size_t, 1> workspace_coordinate{column};
        value =
            std::fma(internal_random_dense::ElementAt(
                         lower_factor,
                         std::span<const std::size_t, 2>(factor_coordinate)),
                     internal_random_dense::ElementAt(
                         sample_workspace,
                         std::span<const std::size_t, 1>(workspace_coordinate)),
                     value);
      }
      if (!std::isfinite(value)) {
        return Status(ErrorCode::kNumerical,
                      "Multivariate normal sample is nonfinite");
      }
      const std::array<std::size_t, 1> workspace_coordinate{row};
      internal_random_dense::ElementAt(
          sample_workspace,
          std::span<const std::size_t, 1>(workspace_coordinate)) = value;
    }
    internal_random_dense::PublishPoint(destination, sample, sample_workspace);
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status FillDenseUnitSphere(const ExecutionContext& context,
                           DenseView<Real, 2> destination, Engine& engine,
                           DenseView<Real, 1> sample_workspace) {
  Status status = internal_random_dense::ValidateSerialHost(
      context, destination, "Unit-sphere sampling requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status = internal_random_dense::ValidateSerialHost(
      context, sample_workspace,
      "Unit-sphere sampling requires serial execution");
  if (!status.ok()) {
    return status;
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  if (sample_workspace.extents()[0] != destination.extents()[1]) {
    return Status(ErrorCode::kShape,
                  "Unit-sphere workspace has the wrong dimension");
  }
  if (sample_count != 0 && dimension_count == 0) {
    return Status(ErrorCode::kShape,
                  "A nonempty unit-sphere sample set needs a dimension");
  }
  if (internal_random_dense::DenseViewsOverlap(destination, sample_workspace)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Unit-sphere destination and workspace overlap");
  }

  if (dimension_count == 1) {
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
      const std::uint64_t word =
          internal_random_distribution::CanonicalBits<32>(engine);
      const std::array<std::size_t, 2> coordinate{sample, 0};
      internal_random_dense::ElementAt(
          destination, std::span<const std::size_t, 2>(coordinate)) =
          (word >> 31U) == 0 ? Real{-1} : Real{1};
    }
    return Status::Ok();
  }
  if (sample_count == 0) {
    return Status::Ok();
  }

  auto distribution = NormalDistribution<Real>::Create(Real{0}, Real{1});
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    bool accepted = false;
    for (std::size_t attempt = 0; attempt < kUnitSphereMaximumAttemptsVersion1;
         ++attempt) {
      Real squared_norm = Real{0};
      for (std::size_t dimension = 0; dimension < dimension_count;
           ++dimension) {
        auto generated = (*distribution)(engine);
        if (!generated.ok()) {
          return generated.status();
        }
        const std::array<std::size_t, 1> coordinate{dimension};
        internal_random_dense::ElementAt(
            sample_workspace, std::span<const std::size_t, 1>(coordinate)) =
            *generated;
        squared_norm = std::fma(*generated, *generated, squared_norm);
      }
      if (!std::isfinite(squared_norm)) {
        return Status(ErrorCode::kNumerical,
                      "Unit-sphere candidate norm is nonfinite");
      }
      if (squared_norm == Real{0}) {
        continue;
      }
      const Real inverse_norm = Real{1} / std::sqrt(squared_norm);
      if (!std::isfinite(inverse_norm)) {
        return Status(ErrorCode::kNumerical,
                      "Unit-sphere normalization is nonfinite");
      }
      for (std::size_t dimension = 0; dimension < dimension_count;
           ++dimension) {
        const std::array<std::size_t, 1> coordinate{dimension};
        Real& value = internal_random_dense::ElementAt(
            sample_workspace, std::span<const std::size_t, 1>(coordinate));
        value *= inverse_norm;
        if (!std::isfinite(value)) {
          return Status(ErrorCode::kNumerical,
                        "Unit-sphere coordinate is nonfinite");
        }
      }
      accepted = true;
      break;
    }
    if (!accepted) {
      return Status(ErrorCode::kNumerical,
                    "Unit-sphere sampling exhausted the attempt limit");
    }
    internal_random_dense::PublishPoint(destination, sample, sample_workspace);
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status FillDenseLatinHypercubeMidpoints(
    const ExecutionContext& context, DenseView<Real, 2> destination,
    Engine& engine, DenseView<std::uint32_t, 2> permutation_workspace) {
  Status status = internal_random_dense::PrepareLatinHypercube(
      context, destination, engine, permutation_workspace);
  if (!status.ok()) {
    return status;
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  if (destination.logical_size() == 0) {
    return Status::Ok();
  }
  const Real denominator = static_cast<Real>(sample_count);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      const std::array<std::size_t, 2> coordinate{sample, dimension};
      const Real value = (static_cast<Real>(internal_random_dense::ElementAt(
                              permutation_workspace,
                              std::span<const std::size_t, 2>(coordinate))) +
                          Real{0.5}) /
                         denominator;
      auto repaired = internal_random_qmc::RepairUnitInterval(
          value, "Dense Latin midpoint produced an invalid result");
      if (!repaired.ok()) {
        return repaired.status();
      }
      internal_random_dense::ElementAt(
          destination, std::span<const std::size_t, 2>(coordinate)) = *repaired;
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real, CanonicalRandomEngine Engine>
Status FillDenseLatinHypercubeJittered(
    const ExecutionContext& context, DenseView<Real, 2> destination,
    Engine& engine, DenseView<std::uint32_t, 2> permutation_workspace) {
  Status status = internal_random_dense::PrepareLatinHypercube(
      context, destination, engine, permutation_workspace);
  if (!status.ok() || destination.logical_size() == 0) {
    return status;
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  const Real denominator = static_cast<Real>(sample_count);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      const std::array<std::size_t, 2> coordinate{sample, dimension};
      const Real unit = internal_random_distribution::UnitReal<Real>(engine);
      const Real stratum = static_cast<Real>(internal_random_dense::ElementAt(
          permutation_workspace, std::span<const std::size_t, 2>(coordinate)));
      Real value = (stratum + unit) / denominator;
      const Real upper = (stratum + Real{1}) / denominator;
      if (value == upper) {
        value = std::nextafter(upper, Real{0});
      }
      auto repaired = internal_random_qmc::RepairUnitInterval(
          value, "Dense Latin jitter produced an invalid result");
      if (!repaired.ok()) {
        return repaired.status();
      }
      internal_random_dense::ElementAt(
          destination, std::span<const std::size_t, 2>(coordinate)) = *repaired;
    }
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status FillDenseHalton(const ExecutionContext& context,
                       DenseView<Real, 2> destination,
                       std::uint64_t initial_index,
                       DenseView<Real, 1> point_workspace) {
  Status status = internal_random_dense::ValidateQmcDestination(
      context, destination, point_workspace, kSobolDimensionCount,
      "Dense Halton fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status =
      internal_random_dense::ValidateQmcIndexRange(destination, initial_index);
  if (!status.ok()) {
    return status;
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    const std::uint64_t index = initial_index + sample;
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      auto value = HaltonCoordinate<Real>(index, dimension);
      if (!value.ok()) {
        return value.status();
      }
      const std::array<std::size_t, 1> coordinate{dimension};
      internal_random_dense::ElementAt(
          point_workspace, std::span<const std::size_t, 1>(coordinate)) =
          *value;
    }
    internal_random_dense::PublishPoint(destination, sample, point_workspace);
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status FillDenseScrambledHalton(
    const ExecutionContext& context, DenseView<Real, 2> destination,
    std::uint64_t initial_index,
    std::span<const std::span<const std::uint32_t>> permutations,
    DenseView<Real, 1> point_workspace) {
  Status status = internal_random_dense::ValidateQmcDestination(
      context, destination, point_workspace, kSobolDimensionCount,
      "Dense scrambled Halton fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status =
      internal_random_dense::ValidateQmcIndexRange(destination, initial_index);
  if (!status.ok()) {
    return status;
  }
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  if (permutations.size() != dimension_count) {
    return Status(ErrorCode::kShape,
                  "Dense Halton digit permutations have the wrong count");
  }
  for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
    const auto prime = PrimeAt(dimension);
    if (!prime.ok()) {
      return prime.status();
    }
    status = internal_random_qmc::ValidateDigitPermutation(
        *prime, permutations[dimension]);
    if (!status.ok()) {
      return status;
    }
    if (internal_random_dense::DenseViewOverlapsSpan(destination,
                                                     permutations[dimension]) ||
        internal_random_dense::DenseViewOverlapsSpan(point_workspace,
                                                     permutations[dimension])) {
      return Status(ErrorCode::kInvalidArgument,
                    "Dense Halton permutation overlaps writable storage");
    }
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    const std::uint64_t index = initial_index + sample;
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      auto value = ScrambledHaltonCoordinate<Real>(index, dimension,
                                                   permutations[dimension]);
      if (!value.ok()) {
        return value.status();
      }
      const std::array<std::size_t, 1> coordinate{dimension};
      internal_random_dense::ElementAt(
          point_workspace, std::span<const std::size_t, 1>(coordinate)) =
          *value;
    }
    internal_random_dense::PublishPoint(destination, sample, point_workspace);
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status FillDenseHammersley(const ExecutionContext& context,
                           DenseView<Real, 2> destination,
                           std::uint64_t initial_index,
                           std::uint64_t total_count,
                           DenseView<Real, 1> point_workspace) {
  Status status = internal_random_dense::ValidateQmcDestination(
      context, destination, point_workspace, kSobolDimensionCount + 1,
      "Dense Hammersley fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  if (total_count == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Dense Hammersley total count must be positive");
  }
  status =
      internal_random_dense::ValidateQmcIndexRange(destination, initial_index);
  if (!status.ok()) {
    return status;
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  if (sample_count != 0 &&
      initial_index + static_cast<std::uint64_t>(sample_count - 1) >=
          total_count) {
    return Status(ErrorCode::kIndex,
                  "Dense Hammersley sample range exceeds total count");
  }
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    const std::uint64_t index = initial_index + sample;
    if (dimension_count != 0) {
      auto leading = internal_random_qmc::RepairUnitInterval(
          static_cast<Real>(index) / static_cast<Real>(total_count),
          "Dense Hammersley leading coordinate is invalid");
      if (!leading.ok()) {
        return leading.status();
      }
      const std::array<std::size_t, 1> coordinate{0};
      internal_random_dense::ElementAt(
          point_workspace, std::span<const std::size_t, 1>(coordinate)) =
          *leading;
    }
    for (std::size_t dimension = 1; dimension < dimension_count; ++dimension) {
      auto prime = PrimeAt(dimension - 1);
      if (!prime.ok()) {
        return prime.status();
      }
      auto value = RadicalInverse<Real>(*prime, index);
      if (!value.ok()) {
        return value.status();
      }
      const std::array<std::size_t, 1> coordinate{dimension};
      internal_random_dense::ElementAt(
          point_workspace, std::span<const std::size_t, 1>(coordinate)) =
          *value;
    }
    internal_random_dense::PublishPoint(destination, sample, point_workspace);
  }
  return Status::Ok();
}

template <SupportedRandomReal Real>
Status FillDenseSobol(const ExecutionContext& context,
                      DenseView<Real, 2> destination,
                      std::uint64_t initial_index,
                      DenseView<Real, 1> point_workspace) {
  Status status = internal_random_dense::ValidateQmcDestination(
      context, destination, point_workspace, kSobolDimensionCount,
      "Dense Sobol fill requires serial execution");
  if (!status.ok()) {
    return status;
  }
  status =
      internal_random_dense::ValidateQmcIndexRange(destination, initial_index);
  if (!status.ok()) {
    return status;
  }
  const auto sample_count = static_cast<std::size_t>(destination.extents()[0]);
  const auto dimension_count =
      static_cast<std::size_t>(destination.extents()[1]);
  for (std::size_t sample = 0; sample < sample_count; ++sample) {
    const std::uint64_t index = initial_index + sample;
    for (std::size_t dimension = 0; dimension < dimension_count; ++dimension) {
      auto value = SobolCoordinate<Real>(index, dimension);
      if (!value.ok()) {
        return value.status();
      }
      const std::array<std::size_t, 1> coordinate{dimension};
      internal_random_dense::ElementAt(
          point_workspace, std::span<const std::size_t, 1>(coordinate)) =
          *value;
    }
    internal_random_dense::PublishPoint(destination, sample, point_workspace);
  }
  return Status::Ok();
}

}  // namespace asc

#endif  // ASC_RANDOM_DENSE_H_
