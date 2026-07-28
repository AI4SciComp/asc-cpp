#ifndef ASC_SRC_CORE_CUDA_RUNTIME_TEST_INTERNAL_H_
#define ASC_SRC_CORE_CUDA_RUNTIME_TEST_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)

#include "asc/core/providers/cuda_export.h"

namespace asc::internal_core_cuda {

enum class CudaRuntimeFault : std::uint8_t {
  kNone,
  kEventCreate,
  kEventRecord,
};

ASC_CORE_CUDA_EXPORT void SetCudaRuntimeFault(CudaRuntimeFault fault) noexcept;
ASC_CORE_CUDA_EXPORT void ResetCudaRuntimeTestState() noexcept;
[[nodiscard]] ASC_CORE_CUDA_EXPORT std::size_t CudaRuntimeDrainCount() noexcept;

}  // namespace asc::internal_core_cuda

#endif  // ASC_CPP_CUDA_RUNTIME_TEST_HOOKS

#endif  // ASC_SRC_CORE_CUDA_RUNTIME_TEST_INTERNAL_H_
