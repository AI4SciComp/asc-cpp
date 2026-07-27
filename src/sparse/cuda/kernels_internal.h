#ifndef ASC_SRC_SPARSE_CUDA_KERNELS_INTERNAL_H_
#define ASC_SRC_SPARSE_CUDA_KERNELS_INTERNAL_H_

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/sparse/providers/cuda.h"

namespace asc {
namespace internal_sparse_cuda {

Status LaunchSparsePointwiseKernel(const ExecutionContext& context,
                                   const PointwisePlan& plan);
Status LaunchStridedSpmvKernel(const ExecutionContext& context,
                               const SpmvPlan& plan);

}  // namespace internal_sparse_cuda
}  // namespace asc

#endif  // ASC_SRC_SPARSE_CUDA_KERNELS_INTERNAL_H_
