#ifndef ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_
#define ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/random/engine.h"
#include "asc/random/providers/dense_cuda_export.h"

namespace asc {

struct ASC_RANDOM_DENSE_CUDA_EXPORT CudaDenseUniform01Generation {
  CompletionEvent completion;
  RandomOffset next_offset;
};

namespace internal_random_dense_cuda {

enum class ScalarType : std::uint8_t {
  kFloat = 0,
  kDouble = 1,
};

struct DenseUniformPlan {
  void* destination = nullptr;
  ScalarType scalar_type = ScalarType::kFloat;
  std::uint8_t rank = 0;
  extent_t shape[8]{};
  stride_t strides[8]{};
  extent_t logical_size = 0;
  RandomStream stream = 0;
  RandomSubsequence subsequence = 0;
  RandomOffset offset = 0;
};

ASC_RANDOM_DENSE_CUDA_EXPORT
Result<CudaDenseUniform01Generation> LaunchDenseUniform01Opaque(
    const ExecutionContext& context, const void* plan);

}  // namespace internal_random_dense_cuda

// Fills a unique CUDA device view in logical dimension-zero-fastest order.
// Success consumes one Philox word per float or two words per double,
// independent of the physical layout. The backing device storage and context
// must remain alive until completion.
template <typename Element, std::size_t Rank>
  requires(std::same_as<Element, float> || std::same_as<Element, double>)
[[nodiscard]] Result<CudaDenseUniform01Generation> CudaFillDenseUniform01(
    const ExecutionContext& context, DenseView<Element, Rank> destination,
    RandomStream stream, RandomSubsequence subsequence, RandomOffset offset) {
  internal_random_dense_cuda::DenseUniformPlan plan;
  plan.destination = destination.data();
  plan.scalar_type = std::same_as<Element, float>
                         ? internal_random_dense_cuda::ScalarType::kFloat
                         : internal_random_dense_cuda::ScalarType::kDouble;
  if constexpr (Rank > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA dense random generation supports rank zero through "
                  "eight");
  } else {
    plan.rank = static_cast<std::uint8_t>(Rank);
    plan.logical_size = destination.mapping().logical_size();
    for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
      plan.shape[dimension] = destination.shape()[dimension];
      plan.strides[dimension] = destination.mapping().strides()[dimension];
    }
  }
  plan.stream = stream;
  plan.subsequence = subsequence;
  plan.offset = offset;
  return internal_random_dense_cuda::LaunchDenseUniform01Opaque(context, &plan);
}

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_DENSE_CUDA_H_
