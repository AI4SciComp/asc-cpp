#include <cuda_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "test_support.h"

int main() {
  if (asc_core_cuda_test::ForceNoCudaDevice()) {
    return asc_core_cuda_test::kSkipReturnCode;
  }
  asc_core_cuda_test::TestContext test;

  auto count = asc::CudaDeviceCount();
  ASC_CORE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }
  if (*count == 0) {
    return asc_core_cuda_test::kSkipReturnCode;
  }

  ASC_CORE_CUDA_EQ(test, cudaSetDevice(0), cudaSuccess);
  int original_device = -1;
  ASC_CORE_CUDA_EQ(test, cudaGetDevice(&original_device), cudaSuccess);

  auto pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto managed = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kManaged);
  auto execution = asc::CreateCudaExecutionContext(0);
  ASC_CORE_CUDA_CHECK(test, pinned.ok());
  ASC_CORE_CUDA_CHECK(test, device.ok());
  ASC_CORE_CUDA_CHECK(test, managed.ok());
  ASC_CORE_CUDA_CHECK(test, execution.ok());

  int observed_device = -1;
  ASC_CORE_CUDA_EQ(test, cudaGetDevice(&observed_device), cudaSuccess);
  ASC_CORE_CUDA_EQ(test, observed_device, original_device);

  if (device.ok()) {
    constexpr std::size_t kImpossibleAllocation =
        std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    auto failure =
        (*device)->Allocate(kImpossibleAllocation, alignof(std::max_align_t));
    ASC_CORE_CUDA_CHECK(test, !failure.ok());
    if (!failure.ok()) {
      ASC_CORE_CUDA_EQ(test, failure.status().code(),
                       asc::ErrorCode::kAllocation);
      ASC_CORE_CUDA_EQ(test, failure.status().provider(), "cuda");
      ASC_CORE_CUDA_CHECK(test, failure.status().native_code() != 0);
    }
  }

  ASC_CORE_CUDA_EQ(test, cudaGetDevice(&observed_device), cudaSuccess);
  ASC_CORE_CUDA_EQ(test, observed_device, original_device);
  return test.Finish();
}
