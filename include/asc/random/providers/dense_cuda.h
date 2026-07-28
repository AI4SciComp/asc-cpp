#ifndef ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_
#define ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_

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

enum class ElementKind {
  kFloat,
  kDouble,
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

template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
struct CudaDenseUniform01Generation {
  CompletionEvent completion;
  RandomOffset next_offset;
};

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
