#ifndef ASC_RANDOM_PROVIDERS_CUDA_H_
#define ASC_RANDOM_PROVIDERS_CUDA_H_

#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/random/engine.h"
#include "asc/random/providers/cuda_export.h"

namespace asc {

struct ASC_RANDOM_CUDA_EXPORT CudaRandomWordGeneration {
  CompletionEvent completion;
  RandomOffset next_offset;
};

// Fills exactly word_count uint32_t values in caller-owned CUDA device
// storage. The result is bit-identical to Philox4x32Word at consecutive
// explicit offsets. The destination and execution context must remain alive
// until completion.
[[nodiscard]] ASC_RANDOM_CUDA_EXPORT Result<CudaRandomWordGeneration>
CudaFillPhilox4x32(const ExecutionContext& context, std::uint32_t* destination,
                   std::uint64_t word_count, RandomStream stream,
                   RandomSubsequence subsequence, RandomOffset offset);

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_CUDA_H_
