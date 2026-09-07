#ifndef ASC_SRC_DENSE_CUDA_CONTEXT_INTERNAL_H_
#define ASC_SRC_DENSE_CUDA_CONTEXT_INTERNAL_H_

#include <cublas_v2.h>

#include <cstdint>
#include <mutex>

#include "asc/dense/providers/cuda.h"

namespace asc::internal_dense_cuda {

Status CublasStatus(cublasStatus_t status, const char* message);

class ContextState {
 public:
  ContextState(std::int32_t device, cublasHandle_t handle) noexcept
      : device_(device), handle_(handle) {}
  ContextState(const ContextState&) = delete;
  ContextState& operator=(const ContextState&) = delete;
  ContextState(ContextState&&) = delete;
  ContextState& operator=(ContextState&&) = delete;
  ~ContextState();

  [[nodiscard]] std::int32_t device() const noexcept { return device_; }
  [[nodiscard]] cublasHandle_t handle() const noexcept { return handle_; }
  [[nodiscard]] std::unique_lock<std::mutex> Lock() {
    return std::unique_lock(handle_mutex_);
  }

 private:
  std::int32_t device_;
  cublasHandle_t handle_;
  std::mutex handle_mutex_;
};

class Access {
 public:
  [[nodiscard]] static ContextState* State(DenseCudaContext& context) noexcept {
    return context.state_.get();
  }
};

}  // namespace asc::internal_dense_cuda

#endif  // ASC_SRC_DENSE_CUDA_CONTEXT_INTERNAL_H_
