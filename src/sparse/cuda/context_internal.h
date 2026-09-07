#ifndef ASC_SRC_SPARSE_CUDA_CONTEXT_INTERNAL_H_
#define ASC_SRC_SPARSE_CUDA_CONTEXT_INTERNAL_H_

#include <cusparse.h>

#include <cstdint>
#include <mutex>

#include "asc/sparse/providers/cuda.h"

namespace asc::internal_sparse_cuda {

Status CusparseStatus(cusparseStatus_t status, const char* message);

class ContextState {
 public:
  ContextState(std::int32_t device, cusparseHandle_t handle) noexcept
      : device_(device), handle_(handle) {}
  ContextState(const ContextState&) = delete;
  ContextState& operator=(const ContextState&) = delete;
  ContextState(ContextState&&) = delete;
  ContextState& operator=(ContextState&&) = delete;
  ~ContextState();

  [[nodiscard]] std::int32_t device() const noexcept { return device_; }
  [[nodiscard]] cusparseHandle_t handle() const noexcept { return handle_; }
  [[nodiscard]] std::mutex& mutex() noexcept { return mutex_; }

 private:
  std::int32_t device_;
  cusparseHandle_t handle_;
  std::mutex mutex_;
};

}  // namespace asc::internal_sparse_cuda

#endif  // ASC_SRC_SPARSE_CUDA_CONTEXT_INTERNAL_H_
