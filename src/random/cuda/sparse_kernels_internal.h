#ifndef ASC_SRC_RANDOM_CUDA_SPARSE_KERNELS_INTERNAL_H_
#define ASC_SRC_RANDOM_CUDA_SPARSE_KERNELS_INTERNAL_H_

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/random/providers/sparse_cuda.h"

namespace asc {
namespace internal_random_sparse_cuda {

Status LaunchSparseUniform01Kernel(const ExecutionContext& context,
                                   const SparseUniformPlan& plan);

}  // namespace internal_random_sparse_cuda
}  // namespace asc

#endif  // ASC_SRC_RANDOM_CUDA_SPARSE_KERNELS_INTERNAL_H_
