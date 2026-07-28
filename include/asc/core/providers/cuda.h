#ifndef ASC_CORE_PROVIDERS_CUDA_H_
#define ASC_CORE_PROVIDERS_CUDA_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda_export.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

ASC_CORE_CUDA_EXPORT Result<std::int32_t> CudaDeviceCount();

class ASC_CORE_CUDA_EXPORT CudaMemoryResource final : public MemoryResource {
 public:
  static Result<std::unique_ptr<CudaMemoryResource>> Create(
      std::int32_t device, MemorySpace memory_space);

  CudaMemoryResource(const CudaMemoryResource&) = delete;
  CudaMemoryResource& operator=(const CudaMemoryResource&) = delete;
  CudaMemoryResource(CudaMemoryResource&&) = delete;
  CudaMemoryResource& operator=(CudaMemoryResource&&) = delete;
  ~CudaMemoryResource() override;

  [[nodiscard]] MemorySpace space() const noexcept override {
    return memory_space_;
  }
  [[nodiscard]] std::int32_t device() const noexcept { return device_; }

  Result<void*> Allocate(std::size_t bytes, std::size_t alignment) override;
  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override;

 private:
  CudaMemoryResource(std::int32_t device, MemorySpace memory_space) noexcept;

  std::int32_t device_;
  MemorySpace memory_space_;
};

ASC_CORE_CUDA_EXPORT Result<ExecutionContext> CreateCudaExecutionContext(
    std::int32_t device, Determinism determinism = Determinism::kDeterministic);

ASC_CORE_CUDA_EXPORT Result<CompletionEvent> RecordCudaEvent(
    const ExecutionContext& context);

}  // namespace asc

#endif  // ASC_CORE_PROVIDERS_CUDA_H_
