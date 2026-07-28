#ifndef ASC_SRC_RANDOM_CUDA_RAW_KERNELS_INTERNAL_H_
#define ASC_SRC_RANDOM_CUDA_RAW_KERNELS_INTERNAL_H_

#include <cstdint>

#include "asc/core/status.h"
#include "asc/random/engine.h"

namespace asc::internal_random_cuda {

Status LaunchPhiloxWords(void* stream, std::uint32_t* destination,
                         std::uint64_t word_count, RandomStream random_stream,
                         RandomSubsequence subsequence, RandomOffset offset);

}  // namespace asc::internal_random_cuda

#endif  // ASC_SRC_RANDOM_CUDA_RAW_KERNELS_INTERNAL_H_
