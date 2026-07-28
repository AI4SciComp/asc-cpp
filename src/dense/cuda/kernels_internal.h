#ifndef ASC_SRC_DENSE_CUDA_KERNELS_INTERNAL_H_
#define ASC_SRC_DENSE_CUDA_KERNELS_INTERNAL_H_

#include "asc/core/status.h"
#include "asc/dense/providers/cuda.h"

namespace asc::internal_dense_cuda {

Status LaunchPointwiseKernel(void* stream, PointwiseOperation operation,
                             OperandDescriptor left, OperandDescriptor right,
                             ViewDescriptor destination);
Status LaunchCopyKernel(void* stream, ViewDescriptor source,
                        ViewDescriptor destination);
Status LaunchScalKernel(void* stream, double alpha, ViewDescriptor destination);
Status LaunchAxpyKernel(void* stream, double alpha, ViewDescriptor source,
                        ViewDescriptor destination);
Status LaunchFillKernel(void* stream, double value, ViewDescriptor destination);

}  // namespace asc::internal_dense_cuda

#endif  // ASC_SRC_DENSE_CUDA_KERNELS_INTERNAL_H_
