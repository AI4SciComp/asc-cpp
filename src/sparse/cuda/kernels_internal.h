#ifndef ASC_SRC_SPARSE_CUDA_KERNELS_INTERNAL_H_
#define ASC_SRC_SPARSE_CUDA_KERNELS_INTERNAL_H_

#include "asc/core/status.h"
#include "asc/sparse/providers/cuda.h"

namespace asc::internal_sparse_cuda {

Status LaunchStridedCsrSpmv(void* stream, double alpha, SparseDescriptor matrix,
                            VectorDescriptor input, double beta,
                            VectorDescriptor output);

Status LaunchSparsePointwise(void* stream, PointwiseOperation operation,
                             OperandDescriptor left, OperandDescriptor right,
                             SparseDescriptor destination);

}  // namespace asc::internal_sparse_cuda

#endif  // ASC_SRC_SPARSE_CUDA_KERNELS_INTERNAL_H_
