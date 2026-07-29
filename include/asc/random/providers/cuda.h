#ifndef ASC_RANDOM_PROVIDERS_CUDA_H_
#define ASC_RANDOM_PROVIDERS_CUDA_H_

#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/random/engine.h"
#include "asc/random/providers/cuda_export.h"

namespace asc {

struct CudaRandomWordGeneration {
  CompletionEvent completion;
  RandomOffset next_offset;
};

[[nodiscard]] ASC_RANDOM_CUDA_EXPORT Result<CudaRandomWordGeneration>
CudaFillPhilox4x32(const ExecutionContext& context,
                   MutableMemoryView destination, std::uint64_t word_count,
                   RandomStream stream, RandomSubsequence subsequence,
                   RandomOffset offset);

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_CUDA_H_
