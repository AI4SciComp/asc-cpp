#ifndef ASC_SRC_RANDOM_CUDA_DENSE_KERNELS_INTERNAL_H_
#define ASC_SRC_RANDOM_CUDA_DENSE_KERNELS_INTERNAL_H_

#include "asc/core/status.h"
#include "asc/random/providers/dense_cuda.h"

namespace asc::internal_random_dense_cuda {

Status LaunchDenseUniform01(void* stream, ViewDescriptor destination,
                            RandomStream random_stream,
                            RandomSubsequence subsequence, RandomOffset offset);

}  // namespace asc::internal_random_dense_cuda

#endif  // ASC_SRC_RANDOM_CUDA_DENSE_KERNELS_INTERNAL_H_
