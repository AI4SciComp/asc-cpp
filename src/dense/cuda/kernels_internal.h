#ifndef ASC_SRC_DENSE_CUDA_KERNELS_INTERNAL_H_
#define ASC_SRC_DENSE_CUDA_KERNELS_INTERNAL_H_

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/providers/cuda.h"

namespace asc {
namespace internal_dense_cuda {

inline constexpr unsigned int kCudaKernelBlockSize = 256;
inline constexpr extent_t kCudaMaximumBlockCount = 65535;

#if defined(__CUDACC__)
__host__ __device__
#endif
    constexpr unsigned int
    CudaLaunchBlockCount(extent_t logical_size) noexcept {
  if (logical_size <= 0) {
    return 0;
  }
  extent_t required_blocks = logical_size / kCudaKernelBlockSize;
  if (logical_size % kCudaKernelBlockSize != 0) {
    ++required_blocks;
  }
  return static_cast<unsigned int>(required_blocks < kCudaMaximumBlockCount
                                       ? required_blocks
                                       : kCudaMaximumBlockCount);
}

#if defined(__CUDACC__)
__host__ __device__
#endif
    constexpr bool
    AdvanceCudaGridStrideOrdinal(extent_t logical_size, extent_t ordinal_stride,
                                 extent_t& ordinal) noexcept {
  if (logical_size <= 0 || ordinal < 0 || ordinal_stride <= 0 ||
      ordinal >= logical_size || logical_size - ordinal <= ordinal_stride) {
    return false;
  }
  ordinal += ordinal_stride;
  return true;
}

Status LaunchPointwiseKernel(const ExecutionContext& execution_context,
                             const PointwisePlan& plan);
Status LaunchBasicLinalgKernel(const ExecutionContext& execution_context,
                               const LinalgPlan& plan);

}  // namespace internal_dense_cuda
}  // namespace asc

#endif  // ASC_SRC_DENSE_CUDA_KERNELS_INTERNAL_H_
