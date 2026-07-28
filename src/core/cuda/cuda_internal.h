#ifndef ASC_SRC_CORE_CUDA_CUDA_INTERNAL_H_
#define ASC_SRC_CORE_CUDA_CUDA_INTERNAL_H_

#include <cuda_runtime_api.h>

#include <cstdint>
#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc::internal_core_cuda {

inline ErrorCode ErrorCodeForCuda(cudaError_t error,
                                  ErrorCode fallback) noexcept {
  switch (error) {
    case cudaErrorMemoryAllocation:
      return ErrorCode::kAllocation;
    case cudaErrorInvalidDevice:
    case cudaErrorNoDevice:
    case cudaErrorInsufficientDriver:
      return ErrorCode::kUnavailable;
    default:
      return fallback;
  }
}

inline Status CudaStatus(cudaError_t error, ErrorCode fallback,
                         const char* message) {
  return Status(ErrorCodeForCuda(error, fallback), message, "cuda",
                static_cast<std::int64_t>(error));
}

class DeviceGuard {
 public:
  static Result<DeviceGuard> Create(std::int32_t device) {
    int previous_device = 0;
    cudaError_t error = cudaGetDevice(&previous_device);
    if (error != cudaSuccess) {
      return CudaStatus(error, ErrorCode::kProvider,
                        "CUDA could not query the current device");
    }
    if (previous_device != device) {
      error = cudaSetDevice(device);
      if (error != cudaSuccess) {
        return CudaStatus(error, ErrorCode::kProvider,
                          "CUDA could not select the requested device");
      }
    }
    return DeviceGuard(previous_device, previous_device != device);
  }

  DeviceGuard(const DeviceGuard&) = delete;
  DeviceGuard& operator=(const DeviceGuard&) = delete;
  DeviceGuard(DeviceGuard&& other) noexcept
      : previous_device_(other.previous_device_),
        changed_(std::exchange(other.changed_, false)) {}
  DeviceGuard& operator=(DeviceGuard&&) = delete;

  ~DeviceGuard() {
    if (changed_) {
      static_cast<void>(cudaSetDevice(previous_device_));
    }
  }

 private:
  DeviceGuard(int previous_device, bool changed) noexcept
      : previous_device_(previous_device), changed_(changed) {}

  int previous_device_;
  bool changed_;
};

}  // namespace asc::internal_core_cuda

#endif  // ASC_SRC_CORE_CUDA_CUDA_INTERNAL_H_
