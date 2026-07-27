#ifndef ASC_RANDOM_DENSE_H_
#define ASC_RANDOM_DENSE_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"

namespace asc {

namespace internal_random_dense {

template <typename Element>
inline constexpr std::uint64_t kWordsPerElement =
    std::same_as<Element, float> ? 1U : 2U;

template <typename Element>
Element GenerateValue(RandomStream stream, RandomSubsequence subsequence,
                      RandomOffset offset) noexcept {
  if constexpr (std::same_as<Element, float>) {
    return Uniform01<float>(Philox4x32Word(stream, subsequence, offset));
  } else {
    return Uniform01<double>(Philox4x32Word(stream, subsequence, offset),
                             Philox4x32Word(stream, subsequence, offset + 1U));
  }
}

}  // namespace internal_random_dense

// Fills a mutable host view from explicit Philox addresses. Logical dimension
// zero varies fastest, independent of the destination's physical layout.
// Success consumes exactly one word per float or two words per double and
// returns the first unused offset. The operation allocates no storage or
// workspace. The caller must exclusively own destination mutation for the
// duration of this synchronous call.
template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<RandomOffset> FillDenseUniform01(
    const ExecutionContext& context, DenseView<Element, Rank> destination,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset) {
  if (context.backend() != Backend::kSerial) {
    return Status(
        ErrorCode::kUnsupported,
        "Milestone 5 dense random generation requires serial execution");
  }
  if (destination.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kUnsupported,
                  "Milestone 5 dense random generation requires host memory");
  }

  auto logical_size =
      CheckedCast<std::uint64_t>(destination.mapping().logical_size());
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  auto word_count = CheckedMultiply(
      *logical_size, internal_random_dense::kWordsPerElement<Element>);
  if (!word_count.ok()) {
    return Status(ErrorCode::kOverflow,
                  "Dense random word consumption exceeds uint64_t");
  }
  auto next_offset = AdvanceRandomOffset(offset, *word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }

  std::array<index_t, Rank> coordinate{};
  for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
    std::uint64_t remaining = ordinal;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      const auto extent =
          static_cast<std::uint64_t>(destination.mapping().shape()[dimension]);
      coordinate[dimension] = static_cast<index_t>(remaining % extent);
      remaining /= extent;
    }

    stride_t physical_offset = 0;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      physical_offset +=
          coordinate[dimension] * destination.mapping().strides()[dimension];
    }
    const RandomOffset value_offset =
        offset + ordinal * internal_random_dense::kWordsPerElement<Element>;
    destination.data()[static_cast<std::size_t>(physical_offset)] =
        internal_random_dense::GenerateValue<Element>(stream, subsequence,
                                                      value_offset);
  }
  return *next_offset;
}

}  // namespace asc

#endif  // ASC_RANDOM_DENSE_H_
