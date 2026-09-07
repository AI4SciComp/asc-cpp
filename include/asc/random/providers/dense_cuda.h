#ifndef ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_
#define ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_

/**
 * @file
 * @brief Public CUDA provider declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_cuda
 */

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/random/engine.h"
#include "asc/random/providers/dense_cuda_export.h"

namespace asc {
namespace internal_random_dense_cuda {

// Preserve the established int-sized erased CUDA descriptor ABI.
// NOLINTNEXTLINE(performance-enum-size)
enum class ElementKind {
  kFloat,   ///< Selects float behavior.
  kDouble,  ///< Selects double behavior.
};

struct ViewDescriptor {
  void* data = nullptr;
  MemorySpace memory_space = MemorySpace::kHost;
  ElementKind element_kind = ElementKind::kFloat;
  std::size_t rank = 0;
  std::array<extent_t, 8> extents{};
  std::array<stride_t, 8> strides{};
  extent_t logical_size = 0;
  std::size_t required_span_size = 0;
};

struct Generation {
  CompletionEvent completion;
  RandomOffset next_offset;
};

ASC_RANDOM_DENSE_CUDA_EXPORT Result<Generation> FillDenseUniform01Erased(
    const ExecutionContext& context, ViewDescriptor destination,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset);

}  // namespace internal_random_dense_cuda

/**
 * @brief Owns an experimental CUDA dense random generation result.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaDenseUniform01Generation {
  /**
   * @brief Stores the completion value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CompletionEvent completion;
  /**
   * @brief Stores the next offset value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  RandomOffset next_offset;
};

/**
 * @brief Enqueues the experimental CUDA FillDenseUniform01 operation and
 * returns completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @tparam Rank Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @param[in] stream CUDA stream whose ordering and lifetime are caller
 * controlled.
 * @param[in] subsequence Deterministic independent subsequence identifier.
 * @param[in] offset Deterministic address offset within the selected sequence.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CudaDenseUniform01Generation<Element, Rank>>
CudaFillDenseUniform01(const ExecutionContext& context,
                       DenseView<Element, Rank> destination,
                       RandomStream stream, RandomSubsequence subsequence,
                       RandomOffset offset) {
  if constexpr (Rank > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA Dense random fill supports ranks zero through eight");
  } else {
    internal_random_dense_cuda::ViewDescriptor descriptor;
    descriptor.data = destination.data();
    descriptor.memory_space = destination.memory_space();
    descriptor.element_kind =
        std::same_as<Element, float>
            ? internal_random_dense_cuda::ElementKind::kFloat
            : internal_random_dense_cuda::ElementKind::kDouble;
    descriptor.rank = Rank;
    descriptor.logical_size = destination.logical_size();
    descriptor.required_span_size = destination.mapping().required_span_size();
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      descriptor.extents[dimension] = destination.extents()[dimension];
      descriptor.strides[dimension] = destination.strides()[dimension];
    }
    auto generation = internal_random_dense_cuda::FillDenseUniform01Erased(
        context, descriptor, stream, subsequence, offset);
    if (!generation.ok()) {
      return generation.status();
    }
    return CudaDenseUniform01Generation<Element, Rank>{
        .completion = std::move(generation->completion),
        .next_offset = generation->next_offset};
  }
}

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_
