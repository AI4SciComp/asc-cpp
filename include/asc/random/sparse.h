#ifndef ASC_RANDOM_SPARSE_H_
#define ASC_RANDOM_SPARSE_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/sparse/coordinate.h"

namespace asc {

namespace internal_random_sparse {

template <typename Element>
inline constexpr std::uint64_t kWordsPerValue =
    std::same_as<Element, float> ? 1U : 2U;

struct Priority {
  std::uint64_t value;
  std::uint64_t ordinal;
};

[[nodiscard]] constexpr bool PriorityLess(Priority left,
                                          Priority right) noexcept {
  return left.value < right.value ||
         (left.value == right.value && left.ordinal < right.ordinal);
}

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

template <std::size_t Rank>
std::array<index_t, Rank> CoordinateFromOrdinal(
    std::uint64_t ordinal, std::span<const extent_t, Rank> shape) noexcept {
  std::array<index_t, Rank> coordinate{};
  for (std::size_t reverse = Rank; reverse > 0; --reverse) {
    const std::size_t dimension = reverse - 1;
    const auto extent = static_cast<std::uint64_t>(shape[dimension]);
    coordinate[dimension] = static_cast<index_t>(ordinal % extent);
    ordinal /= extent;
  }
  return coordinate;
}

[[nodiscard]] inline Priority StructurePriority(
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset,
    std::uint64_t ordinal) noexcept {
  const RandomOffset address = offset + 2U * ordinal;
  const auto high_word =
      static_cast<std::uint64_t>(Philox4x32Word(stream, subsequence, address));
  const auto low_word = static_cast<std::uint64_t>(
      Philox4x32Word(stream, subsequence, address + 1U));
  return Priority{(high_word << 32U) | low_word, ordinal};
}

}  // namespace internal_random_sparse

// Owns one generated canonical coordinate array and the first unused offsets
// in the independent structure and value address domains. The MemoryResource
// passed to GenerateSparseUniform01 must outlive this move-only result.
template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct SparseUniform01Generation {
  CoordinateArray<Element, ExtentsType> array;
  RandomOffset next_structure_offset;
  RandomOffset next_value_offset;
};

// Generates an exact-count deterministic pseudorandom coordinate structure.
// Candidate priorities use two structure words per logical coordinate and are
// ordered by (priority, canonical ordinal). Values then use one word per float
// or two words per double in finalized canonical stored-entry order.
//
// Validation precedes allocation. Success allocates only the coordinate
// builder's declared coordinate and value buffers; selection uses repeated
// scans with O(ExtentsType::kRank) local storage and
// O(exact_count * extents.logical_size()) time. The call is synchronous and
// mutates no shared state.
template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<SparseUniform01Generation<Element, ExtentsType>>
GenerateSparseUniform01(const ExecutionContext& context,
                        const ExtentsType& extents, nnz_t exact_count,
                        MemoryResource& resource, RandomStream structure_stream,
                        RandomSubsequence structure_subsequence,
                        RandomOffset structure_offset,
                        RandomStream value_stream,
                        RandomSubsequence value_subsequence,
                        RandomOffset value_offset) {
  if (context.backend() != Backend::kSerial) {
    return Status(
        ErrorCode::kUnsupported,
        "Milestone 5 sparse random generation requires serial execution");
  }
  if (resource.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kUnsupported,
                  "Milestone 5 sparse random generation requires host memory");
  }
  if (exact_count < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse random exact count cannot be negative");
  }
  if (exact_count > extents.logical_size()) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse random exact count exceeds the logical shape");
  }
  if (structure_stream == value_stream &&
      structure_subsequence == value_subsequence) {
    return Status(
        ErrorCode::kInvalidArgument,
        "Sparse random structure and values require distinct address domains");
  }

  auto logical_size = CheckedCast<std::uint64_t>(extents.logical_size());
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  auto count = CheckedCast<std::uint64_t>(exact_count);
  if (!count.ok()) {
    return count.status();
  }

  std::uint64_t structure_word_count = 0;
  if (exact_count != 0) {
    auto checked_structure_words =
        CheckedMultiply(*logical_size, std::uint64_t{2});
    if (!checked_structure_words.ok()) {
      return Status(ErrorCode::kOverflow,
                    "Sparse structure word consumption exceeds uint64_t");
    }
    structure_word_count = *checked_structure_words;
  }
  auto value_word_count =
      CheckedMultiply(*count, internal_random_sparse::kWordsPerValue<Element>);
  if (!value_word_count.ok()) {
    return Status(ErrorCode::kOverflow,
                  "Sparse value word consumption exceeds uint64_t");
  }
  auto next_structure_offset =
      AdvanceRandomOffset(structure_offset, structure_word_count);
  if (!next_structure_offset.ok()) {
    return next_structure_offset.status();
  }
  auto next_value_offset = AdvanceRandomOffset(value_offset, *value_word_count);
  if (!next_value_offset.ok()) {
    return next_value_offset.status();
  }

  auto builder = CoordinateBuilder<Element, ExtentsType>::Create(
      extents, exact_count, resource);
  if (!builder.ok()) {
    return builder.status();
  }

  internal_random_sparse::Priority previous{};
  bool has_previous = false;
  for (std::uint64_t selected = 0; selected < *count; ++selected) {
    internal_random_sparse::Priority best{};
    bool has_best = false;
    for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
      const auto candidate = internal_random_sparse::StructurePriority(
          structure_stream, structure_subsequence, structure_offset, ordinal);
      if (has_previous &&
          !internal_random_sparse::PriorityLess(previous, candidate)) {
        continue;
      }
      if (!has_best || internal_random_sparse::PriorityLess(candidate, best)) {
        best = candidate;
        has_best = true;
      }
    }
    if (!has_best) {
      return Status(ErrorCode::kInternal,
                    "Sparse priority selection exhausted its candidates");
    }
    const auto coordinate =
        internal_random_sparse::CoordinateFromOrdinal<ExtentsType::kRank>(
            best.ordinal, extents.values());
    const Status add_status = builder->Add(coordinate, Element{});
    if (!add_status.ok()) {
      return add_status;
    }
    previous = best;
    has_previous = true;
  }

  auto array = builder->Finalize(context, DuplicatePolicy::kReject,
                                 ExplicitZeroPolicy::kKeep);
  if (!array.ok()) {
    return array.status();
  }
  auto view = array->view();
  if (!view.ok()) {
    return view.status();
  }
  for (std::uint64_t position = 0; position < *count; ++position) {
    const RandomOffset address =
        value_offset +
        position * internal_random_sparse::kWordsPerValue<Element>;
    view->value_data()[static_cast<std::size_t>(position)] =
        internal_random_sparse::GenerateValue<Element>(
            value_stream, value_subsequence, address);
  }

  return SparseUniform01Generation<Element, ExtentsType>{
      std::move(*array), *next_structure_offset, *next_value_offset};
}

}  // namespace asc

#endif  // ASC_RANDOM_SPARSE_H_
