#ifndef ASC_SRC_RANDOM_CUDA_DENSE_KERNELS_INTERNAL_H_
#define ASC_SRC_RANDOM_CUDA_DENSE_KERNELS_INTERNAL_H_

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/random/providers/dense_cuda.h"

namespace asc {
namespace internal_random_dense_cuda {

Status LaunchDenseUniform01Kernel(const ExecutionContext& context,
                                  const DenseUniformPlan& plan);

}  // namespace internal_random_dense_cuda
}  // namespace asc

#endif  // ASC_SRC_RANDOM_CUDA_DENSE_KERNELS_INTERNAL_H_
