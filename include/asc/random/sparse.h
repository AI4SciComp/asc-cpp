#ifndef ASC_RANDOM_SPARSE_H_
#define ASC_RANDOM_SPARSE_H_

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"

namespace asc {

inline constexpr std::uint32_t kRandomSparseAdapterSequenceVersion1 = 1;

struct SparseRandomStructureCandidate {
  std::uint64_t priority = 0;
  std::uint64_t ordinal = 0;

  friend bool operator==(const SparseRandomStructureCandidate&,
                         const SparseRandomStructureCandidate&) = default;
};

template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct SparseUniform01Generation {
  CoordinateArray<Element, ExtentsType> array;
  RandomOffset next_structure_offset;
  RandomOffset next_value_offset;
};

namespace internal_random_sparse {

template <typename Generator, typename Element>
concept ValueGeneratorFor = requires(Generator& generator) {
  { generator() } -> std::same_as<Result<Element>>;
};

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

inline bool CandidateLess(
    const SparseRandomStructureCandidate& left,
    const SparseRandomStructureCandidate& right) noexcept {
  return PriorityOrdinalLess(left.priority, left.ordinal, right.priority,
                             right.ordinal);
}

inline bool ByteSpansOverlap(const void* left, std::size_t left_size,
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

template <typename Element, typename View>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
Result<RandomOffset> FillUniform01Values(const ExecutionContext& context,
                                         View destination, RandomStream stream,
                                         RandomSubsequence subsequence,
                                         RandomOffset offset) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Sparse value fill requires serial execution");
  }
  if (destination.memory_space() != MemorySpace::kHost ||
      !context.CanAccess(destination.memory_space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse value fill requires host storage");
  }
  auto count = CheckedCast<std::uint64_t>(destination.nnz());
  if (!count.ok()) {
    return count.status();
  }
  constexpr std::uint64_t kWordsPerElement = ValueWordsPerElement<Element>();
  auto word_count = CheckedMultiply(*count, kWordsPerElement);
  if (!word_count.ok()) {
    return word_count.status();
  }
  auto next_offset = AdvanceRandomOffset(offset, *word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }
  for (std::uint64_t position = 0; position < *count; ++position) {
    destination.values()[static_cast<std::size_t>(position)] =
        GenerateUniform01<Element>(stream, subsequence,
                                   offset + position * kWordsPerElement);
  }
  return *next_offset;
}

template <typename Element, typename View, typename Generator>
  requires ValueGeneratorFor<Generator, Element>
Status FillPseudoValues(const ExecutionContext& context, View destination,
                        Generator& generator) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Sparse pseudo fill requires serial execution");
  }
  if (destination.memory_space() != MemorySpace::kHost ||
      !context.CanAccess(destination.memory_space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse pseudo fill requires host storage");
  }
  auto count = CheckedCast<std::size_t>(destination.nnz());
  if (!count.ok()) {
    return count.status();
  }
  for (std::size_t position = 0; position < *count; ++position) {
    auto generated = generator();
    if (!generated.ok()) {
      return generated.status();
    }
    destination.values()[position] = *generated;
  }
  return Status::Ok();
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

template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<RandomOffset> FillSparseUniform01(
    const ExecutionContext& context, CoordinateView<Element, Rank> destination,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset) {
  return internal_random_sparse::FillUniform01Values<Element>(
      context, destination, stream, subsequence, offset);
}

template <typename Element, SparseCompressedFormat Format>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<RandomOffset> FillSparseUniform01(
    const ExecutionContext& context,
    CompressedSparseView<Element, Format> destination, RandomStream stream,
    RandomSubsequence subsequence, RandomOffset offset) {
  return internal_random_sparse::FillUniform01Values<Element>(
      context, destination, stream, subsequence, offset);
}

template <typename Element, std::size_t Rank, typename Generator>
  requires internal_random_sparse::ValueGeneratorFor<Generator, Element>
Status FillSparsePseudo(const ExecutionContext& context,
                        CoordinateView<Element, Rank> destination,
                        Generator& generator) {
  return internal_random_sparse::FillPseudoValues<Element>(context, destination,
                                                           generator);
}

template <typename Element, SparseCompressedFormat Format, typename Generator>
  requires internal_random_sparse::ValueGeneratorFor<Generator, Element>
Status FillSparsePseudo(const ExecutionContext& context,
                        CompressedSparseView<Element, Format> destination,
                        Generator& generator) {
  return internal_random_sparse::FillPseudoValues<Element>(context, destination,
                                                           generator);
}

template <SparseExtents ExtentsType>
[[nodiscard]] Result<RandomOffset> GenerateSparseStructure(
    const ExecutionContext& context, ExtentsType extents, nnz_t exact_count,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset,
    std::span<SparseRandomStructureCandidate> candidate_workspace,
    std::span<std::uint64_t> output_ordinals) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Sparse structure generation requires serial execution");
  }
  if (!context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse structure generation requires host access");
  }
  if (exact_count < 0 || exact_count > extents.logical_size()) {
    return Status(ErrorCode::kShape,
                  "Sparse structure count is incompatible with the shape");
  }
  auto logical_size = CheckedCast<std::uint64_t>(extents.logical_size());
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  auto count = CheckedCast<std::uint64_t>(exact_count);
  if (!count.ok()) {
    return count.status();
  }
  if (output_ordinals.size() != static_cast<std::size_t>(*count)) {
    return Status(ErrorCode::kShape,
                  "Sparse structure output has the wrong count");
  }
  const std::size_t required_candidates =
      *count == 0 ? 0 : static_cast<std::size_t>(*logical_size);
  if (candidate_workspace.size() != required_candidates) {
    return Status(ErrorCode::kShape,
                  "Sparse structure candidate workspace has the wrong size");
  }
  if (internal_random_sparse::ByteSpansOverlap(
          candidate_workspace.data(), candidate_workspace.size_bytes(),
          output_ordinals.data(), output_ordinals.size_bytes())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse structure workspace and output overlap");
  }
  std::uint64_t word_count = 0;
  if (*count != 0) {
    auto checked_words = CheckedMultiply<std::uint64_t>(*logical_size, 2);
    if (!checked_words.ok()) {
      return checked_words.status();
    }
    word_count = *checked_words;
  }
  auto next_offset = AdvanceRandomOffset(offset, word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }
  if (*count == 0) {
    return *next_offset;
  }

  for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
    candidate_workspace[static_cast<std::size_t>(ordinal)] = {
        .priority = internal_random_sparse::Priority(stream, subsequence,
                                                     offset + 2U * ordinal),
        .ordinal = ordinal};
  }
  if (*count != *logical_size) {
    std::nth_element(
        candidate_workspace.begin(),
        candidate_workspace.begin() + static_cast<std::ptrdiff_t>(*count),
        candidate_workspace.end(), internal_random_sparse::CandidateLess);
  }
  SparseRandomStructureCandidate cutoff = candidate_workspace[0];
  for (std::uint64_t position = 1; position < *count; ++position) {
    const auto candidate =
        candidate_workspace[static_cast<std::size_t>(position)];
    if (internal_random_sparse::CandidateLess(cutoff, candidate)) {
      cutoff = candidate;
    }
  }

  std::size_t selected = 0;
  for (std::uint64_t ordinal = 0; ordinal < *logical_size; ++ordinal) {
    const SparseRandomStructureCandidate candidate{
        .priority = internal_random_sparse::Priority(stream, subsequence,
                                                     offset + 2U * ordinal),
        .ordinal = ordinal};
    if (!internal_random_sparse::CandidateLess(cutoff, candidate)) {
      output_ordinals[selected++] = ordinal;
    }
  }
  if (selected != output_ordinals.size()) {
    return Status(ErrorCode::kInternal,
                  "Sparse structure selection produced the wrong count");
  }
  return *next_offset;
}

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
