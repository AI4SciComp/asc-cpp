#ifndef ASC_SRC_RANDOM_CUDA_SPARSE_KERNELS_INTERNAL_H_
#define ASC_SRC_RANDOM_CUDA_SPARSE_KERNELS_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <span>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/providers/sparse_cuda.h"

namespace asc::internal_random_sparse_cuda {

#if defined(__CUDACC__)
#define ASC_RANDOM_SPARSE_CUDA_HOST_DEVICE __host__ __device__
#else
#define ASC_RANDOM_SPARSE_CUDA_HOST_DEVICE
#endif

[[nodiscard]] ASC_RANDOM_SPARSE_CUDA_HOST_DEVICE constexpr bool
PriorityOrdinalLess(std::uint64_t left_priority, std::uint64_t left_ordinal,
                    std::uint64_t right_priority,
                    std::uint64_t right_ordinal) noexcept {
  return left_priority < right_priority ||
         (left_priority == right_priority && left_ordinal < right_ordinal);
}

#undef ASC_RANDOM_SPARSE_CUDA_HOST_DEVICE

struct SparseDescriptor {
  index_t* coordinates = nullptr;
  void* values = nullptr;
  ElementKind element_kind = ElementKind::kFloat;
  std::size_t rank = 0;
  std::uint64_t logical_size = 0;
  std::uint64_t exact_count = 0;
};

Status LaunchSparseUniform01(void* stream, SparseDescriptor destination,
                             std::span<const extent_t> extents,
                             RandomStream structure_stream,
                             RandomSubsequence structure_subsequence,
                             RandomOffset structure_offset,
                             RandomStream value_stream,
                             RandomSubsequence value_subsequence,
                             RandomOffset value_offset);

}  // namespace asc::internal_random_sparse_cuda

#endif  // ASC_SRC_RANDOM_CUDA_SPARSE_KERNELS_INTERNAL_H_
