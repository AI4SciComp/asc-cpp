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

template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct SparseUniform01Generation {
  CoordinateArray<Element, ExtentsType> array;
  RandomOffset next_structure_offset;
  RandomOffset next_value_offset;
};

namespace internal_random_sparse {

template <typename Element>
constexpr std::uint64_t ValueWordsPerElement() noexcept {
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

inline bool PriorityOrdinalLess(std::uint64_t left_priority,
                                std::uint64_t left_ordinal,
                                std::uint64_t right_priority,
                                std::uint64_t right_ordinal) noexcept {
  return left_priority < right_priority ||
         (left_priority == right_priority && left_ordinal < right_ordinal);
}

inline std::uint64_t Priority(RandomStream stream,
                              RandomSubsequence subsequence,
                              RandomOffset offset) noexcept {
  const auto high_word = GeneratePhilox4x32Word(stream, subsequence, offset);
  const auto low_word = GeneratePhilox4x32Word(stream, subsequence, offset + 1);
  return (static_cast<std::uint64_t>(high_word) << 32U) |
         static_cast<std::uint64_t>(low_word);
}

template <SparseExtents ExtentsType>
std::array<index_t, ExtentsType::kRank> CoordinateFromOrdinal(
    std::uint64_t ordinal, const ExtentsType& extents) noexcept {
  std::array<index_t, ExtentsType::kRank> coordinate{};
  for (std::size_t reverse = ExtentsType::kRank; reverse > 0; --reverse) {
    const std::size_t dimension = reverse - 1;
    const auto extent = static_cast<std::uint64_t>(extents.values()[dimension]);
    coordinate[dimension] = static_cast<index_t>(ordinal % extent);
    ordinal /= extent;
  }
  return coordinate;
}

}  // namespace internal_random_sparse

template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<SparseUniform01Generation<Element, ExtentsType>>
GenerateSparseUniform01(const ExecutionContext& context, ExtentsType extents,
                        nnz_t exact_count, MemoryResource& resource,
                        RandomStream structure_stream,
                        RandomSubsequence structure_subsequence,
                        RandomOffset structure_offset,
                        RandomStream value_stream,
                        RandomSubsequence value_subsequence,
                        RandomOffset value_offset) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Sparse random generation requires serial execution");
  }
  if (!context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse random generation requires host memory access");
  }
  if (resource.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kUnsupported,
                  "Sparse random generation requires a host resource");
  }
  if (exact_count < 0 || exact_count > extents.logical_size()) {
    return Status(ErrorCode::kShape,
                  "Sparse exact count is incompatible with the shape");
  }
  if (structure_stream == value_stream &&
      structure_subsequence == value_subsequence) {
    return Status(
        ErrorCode::kInvalidArgument,
        "Sparse structure and values require distinct random domains");
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
        CheckedMultiply<std::uint64_t>(*logical_size, 2);
    if (!checked_structure_words.ok()) {
      return checked_structure_words.status();
    }
    structure_word_count = *checked_structure_words;
  }
  constexpr std::uint64_t kValueWordsPerElement =
      internal_random_sparse::ValueWordsPerElement<Element>();
  auto value_word_count =
      CheckedMultiply<std::uint64_t>(*count, kValueWordsPerElement);
  if (!value_word_count.ok()) {
    return value_word_count.status();
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
      resource, extents, exact_count);
  if (!builder.ok()) {
    return builder.status();
  }

  bool has_previous = false;
  std::uint64_t previous_priority = 0;
  std::uint64_t previous_ordinal = 0;
  for (std::uint64_t selected = 0; selected < *count; ++selected) {
    bool found = false;
    std::uint64_t best_priority = 0;
    std::uint64_t best_ordinal = 0;
    for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
      const RandomOffset priority_offset = structure_offset + 2U * ordinal;
      const std::uint64_t priority = internal_random_sparse::Priority(
          structure_stream, structure_subsequence, priority_offset);
      if (has_previous &&
          !internal_random_sparse::PriorityOrdinalLess(
              previous_priority, previous_ordinal, priority, ordinal)) {
        continue;
      }
      if (!found || internal_random_sparse::PriorityOrdinalLess(
                        priority, ordinal, best_priority, best_ordinal)) {
        found = true;
        best_priority = priority;
        best_ordinal = ordinal;
      }
    }

    const auto coordinate =
        internal_random_sparse::CoordinateFromOrdinal(best_ordinal, extents);
    Status add_status = builder->Add(
        std::span<const index_t, ExtentsType::kRank>(coordinate), Element{});
    if (!add_status.ok()) {
      return add_status;
    }
    has_previous = true;
    previous_priority = best_priority;
    previous_ordinal = best_ordinal;
  }

  auto array = std::move(*builder).Finalize(context, DuplicatePolicy::kReject,
                                            ExplicitZeroPolicy::kKeep);
  if (!array.ok()) {
    return array.status();
  }
  auto view = array->view();
  if (!view.ok()) {
    return view.status();
  }
  for (std::uint64_t position = 0; position < *count; ++position) {
    const RandomOffset word_offset =
        value_offset + position * kValueWordsPerElement;
    view->values()[static_cast<std::size_t>(position)] =
        internal_random_sparse::GenerateUniform01<Element>(
            value_stream, value_subsequence, word_offset);
  }

  return SparseUniform01Generation<Element, ExtentsType>{
      .array = std::move(*array),
      .next_structure_offset = *next_structure_offset,
      .next_value_offset = *next_value_offset};
}

}  // namespace asc

#endif  // ASC_RANDOM_SPARSE_H_
