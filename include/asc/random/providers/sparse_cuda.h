#ifndef ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_
#define ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/engine.h"
#include "asc/random/providers/sparse_cuda_export.h"
#include "asc/sparse/coordinate.h"

namespace asc {

template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaSparseUniform01Generation {
  CoordinateArray<Element, ExtentsType> array;
  CompletionEvent completion;
  RandomOffset next_structure_offset;
  RandomOffset next_value_offset;
};

namespace internal_random_sparse_cuda {

enum class ScalarType : std::uint8_t {
  kFloat = 0,
  kDouble = 1,
};

struct SparseUniformPlan {
  index_t* coordinates = nullptr;
  void* values = nullptr;
  ScalarType scalar_type = ScalarType::kFloat;
  std::uint8_t rank = 0;
  extent_t shape[8]{};
  extent_t logical_size = 0;
  nnz_t exact_count = 0;
  RandomStream structure_stream = 0;
  RandomSubsequence structure_subsequence = 0;
  RandomOffset structure_offset = 0;
  RandomStream value_stream = 0;
  RandomSubsequence value_subsequence = 0;
  RandomOffset value_offset = 0;
};

ASC_RANDOM_SPARSE_CUDA_EXPORT
Result<CompletionEvent> LaunchSparseUniform01Opaque(
    const ExecutionContext& context, const void* plan);

}  // namespace internal_random_sparse_cuda

// Generates exactly exact_count canonical coordinates and Uniform01 values in
// caller-selected CUDA device storage. Selection and word consumption are
// bit-identical to GenerateSparseUniform01. The implementation uses no hidden
// workspace. It performs
// O(exact_count * logical_size + exact_count^2 * rank) work in a deliberately
// low-workspace single-thread kernel and makes exactly two result-storage
// allocations. The context, resource, and returned array and backing storage
// must remain alive until completion; a view does not extend owner lifetime.
template <typename Element, SparseExtents ExtentsType>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CudaSparseUniform01Generation<Element, ExtentsType>>
CudaGenerateSparseUniform01(
    const ExecutionContext& context, const ExtentsType& extents,
    nnz_t exact_count, MemoryResource& resource, RandomStream structure_stream,
    RandomSubsequence structure_subsequence, RandomOffset structure_offset,
    RandomStream value_stream, RandomSubsequence value_subsequence,
    RandomOffset value_offset) {
  if constexpr (ExtentsType::kRank > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA sparse random generation supports rank zero through "
                  "eight");
  }
  if (context.backend() != Backend::kCuda ||
      context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse random generation requires a CUDA context");
  }
  if (resource.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA sparse random generation requires device memory");
  }
  if (exact_count < 0 || exact_count > extents.logical_size()) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse random exact count is outside the shape");
  }
  if (structure_stream == value_stream &&
      structure_subsequence == value_subsequence) {
    return Status(
        ErrorCode::kInvalidArgument,
        "CUDA sparse structure and values require distinct address domains");
  }
  auto logical_size = CheckedCast<std::uint64_t>(extents.logical_size());
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  auto count = CheckedCast<std::uint64_t>(exact_count);
  if (!count.ok()) {
    return count.status();
  }
  std::uint64_t structure_words = 0;
  if (exact_count != 0) {
    auto checked = CheckedMultiply(*logical_size, std::uint64_t{2});
    if (!checked.ok()) {
      return Status(ErrorCode::kOverflow,
                    "CUDA sparse structure word consumption overflowed");
    }
    structure_words = *checked;
  }
  constexpr std::uint64_t kWordsPerValue =
      std::same_as<Element, float> ? 1U : 2U;
  auto value_words = CheckedMultiply(*count, kWordsPerValue);
  if (!value_words.ok()) {
    return Status(ErrorCode::kOverflow,
                  "CUDA sparse value word consumption overflowed");
  }
  auto next_structure = AdvanceRandomOffset(structure_offset, structure_words);
  if (!next_structure.ok()) {
    return next_structure.status();
  }
  auto next_value = AdvanceRandomOffset(value_offset, *value_words);
  if (!next_value.ok()) {
    return next_value.status();
  }
  auto array = internal_sparse_coordinate::ArrayFactory<
      Element, ExtentsType>::AllocateStorage(extents, exact_count, resource);
  if (!array.ok()) {
    return array.status();
  }
  auto view = array->view();
  if (!view.ok()) {
    return view.status();
  }

  internal_random_sparse_cuda::SparseUniformPlan plan;
  plan.coordinates = const_cast<index_t*>(view->coordinate_data());
  plan.values = view->value_data();
  plan.scalar_type = std::same_as<Element, float>
                         ? internal_random_sparse_cuda::ScalarType::kFloat
                         : internal_random_sparse_cuda::ScalarType::kDouble;
  plan.rank = static_cast<std::uint8_t>(ExtentsType::kRank);
  for (std::size_t dimension = 0; dimension < ExtentsType::kRank; ++dimension) {
    plan.shape[dimension] = extents.values()[dimension];
  }
  plan.logical_size = extents.logical_size();
  plan.exact_count = exact_count;
  plan.structure_stream = structure_stream;
  plan.structure_subsequence = structure_subsequence;
  plan.structure_offset = structure_offset;
  plan.value_stream = value_stream;
  plan.value_subsequence = value_subsequence;
  plan.value_offset = value_offset;
  auto completion =
      internal_random_sparse_cuda::LaunchSparseUniform01Opaque(context, &plan);
  if (!completion.ok()) {
    return completion.status();
  }
  return CudaSparseUniform01Generation<Element, ExtentsType>{
      std::move(*array), std::move(*completion), *next_structure, *next_value};
}

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_SPARSE_CUDA_H_
