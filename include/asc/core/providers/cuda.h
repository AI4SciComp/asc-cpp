#ifndef ASC_CORE_PROVIDERS_CUDA_H_
#define ASC_CORE_PROVIDERS_CUDA_H_

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

#include <cstddef>
#include <cstdint>
#include <memory>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda_export.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

/**
 * @brief Enqueues the experimental CUDA DeviceCount operation and returns
 * completion.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
ASC_CORE_CUDA_EXPORT Result<std::int32_t> CudaDeviceCount();

/**
 * @brief Allocates CUDA device or pinned-host memory for one device.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 * @ingroup asc_cuda
 */
class ASC_CORE_CUDA_EXPORT CudaMemoryResource final : public MemoryResource {
 public:
  /**
   * @brief Validates inputs and creates the requested CUDA provider object.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] device The device value required by this contract.
   * @param[in] memory_space Placement of every referenced storage byte.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
  static Result<std::unique_ptr<CudaMemoryResource>> Create(
      std::int32_t device, MemorySpace memory_space);

  /**
   * @brief Constructs a CudaMemoryResource with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaMemoryResource(const CudaMemoryResource&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaMemoryResource& operator=(const CudaMemoryResource&) = delete;
  /**
   * @brief Constructs a CudaMemoryResource with the documented ownership and
   * validity state.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  CudaMemoryResource(CudaMemoryResource&&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  CudaMemoryResource& operator=(CudaMemoryResource&&) = delete;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   * @ingroup asc_cuda
   */
  ~CudaMemoryResource() override;

  /**
   * @brief Returns the object's space contract value.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] MemorySpace space() const noexcept override {
    return memory_space_;
  }
  /**
   * @brief Performs the public device operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_cuda
   */
  [[nodiscard]] std::int32_t device() const noexcept { return device_; }

  /**
   * @brief Performs the public Allocate operation defined by the CUDA provider
   * contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_cuda
   */
  Result<void*> Allocate(std::size_t bytes, std::size_t alignment) override;
  /**
   * @brief Performs the public Deallocate operation defined by the CUDA
   * provider contract.
   *
   * This API is experimental in 0.9.0. CUDA storage, context, stream,
   * event, and workspace owners must remain alive until returned completion
   * has been ordered or waited. Enqueue success does not imply completion.
   *
   * @param[in] pointer The pointer value required by this contract.
   * @param[in] bytes Requested byte count.
   * @param[in] alignment Power-of-two allocation alignment.
   * @ingroup asc_cuda
   */
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override;

 private:
  CudaMemoryResource(std::int32_t device, MemorySpace memory_space) noexcept;

  std::int32_t device_;
  MemorySpace memory_space_;
};

/**
 * @brief Validates inputs and creates the requested CUDA provider object.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] device The device value required by this contract.
 * @param[in] determinism The determinism value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
ASC_CORE_CUDA_EXPORT Result<ExecutionContext> CreateCudaExecutionContext(
    std::int32_t device, Determinism determinism = Determinism::kDeterministic);

/**
 * @brief Performs the public RecordCudaEvent operation defined by the CUDA
 * provider contract.
 *
 * This API is experimental in 0.9.0. CUDA storage, context, stream,
 * event, and workspace owners must remain alive until returned completion
 * has been ordered or waited. Enqueue success does not imply completion.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_cuda
 */
ASC_CORE_CUDA_EXPORT Result<CompletionEvent> RecordCudaEvent(
    const ExecutionContext& context);

}  // namespace asc

#endif  // ASC_CORE_PROVIDERS_CUDA_H_
