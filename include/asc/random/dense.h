#ifndef ASC_RANDOM_DENSE_H_
#define ASC_RANDOM_DENSE_H_

#include <concepts>
#include <cstddef>
#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"

namespace asc {

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

}  // namespace internal_random_dense

template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<RandomOffset> FillDenseUniform01(
    const ExecutionContext& context, DenseView<Element, Rank> destination,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Dense random fill requires serial execution");
  }
  if (destination.memory_space() != MemorySpace::kHost ||
      !context.CanAccess(destination.memory_space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Dense random fill requires host-accessible storage");
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
    std::uint64_t remaining = ordinal;
    std::uint64_t physical_offset = 0;
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      const auto extent =
          static_cast<std::uint64_t>(destination.extents()[dimension]);
      const std::uint64_t coordinate = remaining % extent;
      remaining /= extent;
      physical_offset += coordinate * static_cast<std::uint64_t>(
                                          destination.strides()[dimension]);
    }

    const RandomOffset word_offset = offset + ordinal * kWordsPerElement;
    destination.data()[static_cast<std::size_t>(physical_offset)] =
        internal_random_dense::GenerateUniform01<Element>(stream, subsequence,
                                                          word_offset);
  }
  return *next_offset;
}

}  // namespace asc

#endif  // ASC_RANDOM_DENSE_H_
