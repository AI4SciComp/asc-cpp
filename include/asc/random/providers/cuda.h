#ifndef ASC_RANDOM_PROVIDERS_CUDA_H_
#define ASC_RANDOM_PROVIDERS_CUDA_H_

/**
 * @file
 * @brief Public CUDA provider declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_cuda
 */

#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/random/engine.h"
#include "asc/random/providers/cuda_export.h"

namespace asc {

/**
 * @brief Owns an experimental CUDA random-word generation result.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
struct CudaRandomWordGeneration {
  /**
   * @brief Stores the completion value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  CompletionEvent completion;
  /**
   * @brief Stores the next offset value for this contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @ingroup asc_cuda
   */
  RandomOffset next_offset;
};

/**
 * @brief Enqueues the experimental CUDA FillPhilox4x32 operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @param[in] word_count The word count value required by this contract.
 * @param[in] stream CUDA stream whose ordering and lifetime are caller
 * controlled.
 * @param[in] subsequence Deterministic independent subsequence identifier.
 * @param[in] offset Deterministic address offset within the selected sequence.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
[[nodiscard]] ASC_RANDOM_CUDA_EXPORT Result<CudaRandomWordGeneration>
CudaFillPhilox4x32(const ExecutionContext& context,
                   MutableMemoryView destination, std::uint64_t word_count,
                   RandomStream stream, RandomSubsequence subsequence,
                   RandomOffset offset);

}  // namespace asc

#endif  // ASC_RANDOM_PROVIDERS_CUDA_H_
