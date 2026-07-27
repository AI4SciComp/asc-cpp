#ifndef ASC_SRC_CORE_CUDA_PROVIDER_INTERNAL_H_
#define ASC_SRC_CORE_CUDA_PROVIDER_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda_export.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {
namespace internal_core_cuda {

class PendingCudaEventState;

class ASC_CORE_CUDA_EXPORT CudaDeviceGuard {
 public:
  static Result<CudaDeviceGuard> Create(const ExecutionContext& context);
  static Result<CudaDeviceGuard> Create(std::int32_t device_ordinal);

  CudaDeviceGuard(const CudaDeviceGuard&) = delete;
  CudaDeviceGuard& operator=(const CudaDeviceGuard&) = delete;
  CudaDeviceGuard(CudaDeviceGuard&& other) noexcept;
  CudaDeviceGuard& operator=(CudaDeviceGuard&& other) = delete;
  ~CudaDeviceGuard();

 private:
  CudaDeviceGuard(std::int32_t original_device, bool restore) noexcept;

  std::int32_t original_device_;
  bool restore_;
};

class ASC_CORE_CUDA_EXPORT PendingCudaEvent {
 public:
  static Result<PendingCudaEvent> Create(
      const ExecutionContext& execution_context);

  PendingCudaEvent(const PendingCudaEvent&) = delete;
  PendingCudaEvent& operator=(const PendingCudaEvent&) = delete;
  PendingCudaEvent(PendingCudaEvent&& other) noexcept;
  PendingCudaEvent& operator=(PendingCudaEvent&& other) noexcept;
  ~PendingCudaEvent();

  [[nodiscard]] Result<CompletionEvent> Record();

 private:
  explicit PendingCudaEvent(
      std::unique_ptr<PendingCudaEventState> state) noexcept;

  std::unique_ptr<PendingCudaEventState> state_;
};

ASC_CORE_CUDA_EXPORT Result<void*> CudaStreamHandle(
    const ExecutionContext& context);

ASC_CORE_CUDA_EXPORT Status ValidateCudaMemory(const ExecutionContext& context,
                                               const void* pointer,
                                               std::size_t bytes,
                                               MemorySpace space);

}  // namespace internal_core_cuda
}  // namespace asc

#endif  // ASC_SRC_CORE_CUDA_PROVIDER_INTERNAL_H_
