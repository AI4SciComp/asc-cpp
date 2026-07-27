#ifndef ASC_SRC_RANDOM_CUDA_KERNELS_INTERNAL_H_
#define ASC_SRC_RANDOM_CUDA_KERNELS_INTERNAL_H_

#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/random/engine.h"

namespace asc {
namespace internal_random_cuda {

struct RawPhiloxPlan {
  std::uint32_t* destination = nullptr;
  std::uint64_t word_count = 0;
  RandomStream stream = 0;
  RandomSubsequence subsequence = 0;
  RandomOffset offset = 0;
};

Status LaunchRawPhiloxKernel(const ExecutionContext& context,
                             const RawPhiloxPlan& plan);

}  // namespace internal_random_cuda
}  // namespace asc

#endif  // ASC_SRC_RANDOM_CUDA_KERNELS_INTERNAL_H_
