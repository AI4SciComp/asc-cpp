#include <cuda_runtime_api.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../src/random/cuda/sparse_kernels_internal.h"
#include "../random_cuda/test_support.h"

namespace {

constexpr std::uint64_t kCollidingPriority = UINT64_C(0x5a5a5a5a5a5a5a5a);
constexpr std::size_t kCount = 4;

__global__ void SortCollidingOrdinals(std::uint64_t* ordinals) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  for (std::size_t position = 1; position < kCount; ++position) {
    const std::uint64_t value = ordinals[position];
    std::size_t insertion = position;
    while (insertion > 0 &&
           asc::internal_random_sparse_cuda::PriorityOrdinalLess(
               kCollidingPriority, value, kCollidingPriority,
               ordinals[insertion - 1])) {
      ordinals[insertion] = ordinals[insertion - 1];
      --insertion;
    }
    ordinals[insertion] = value;
  }
}

}  // namespace

int main() {
  static_assert(asc::internal_random_sparse_cuda::PriorityOrdinalLess(
      kCollidingPriority, 1, kCollidingPriority, 2));
  static_assert(!asc::internal_random_sparse_cuda::PriorityOrdinalLess(
      kCollidingPriority, 2, kCollidingPriority, 1));
  static_assert(!asc::internal_random_sparse_cuda::PriorityOrdinalLess(
      kCollidingPriority, 2, kCollidingPriority, 2));

  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  asc_random_cuda_test::TestContext test;
  ASC_M7_CUDA_EQ(test, cudaSetDevice(asc_random_cuda_test::kCudaDevice),
                 cudaSuccess);

  constexpr std::array<std::uint64_t, kCount> kInput{7, 1, 5, 2};
  constexpr std::array<std::uint64_t, kCount> kExpected{1, 2, 5, 7};
  void* allocation_pointer = nullptr;
  const cudaError_t allocation =
      cudaMalloc(&allocation_pointer, kCount * sizeof(std::uint64_t));
  ASC_M7_CUDA_EQ(test, allocation, cudaSuccess);
  if (allocation != cudaSuccess) {
    return test.Finish();
  }
  auto* device_ordinals = static_cast<std::uint64_t*>(allocation_pointer);

  cudaError_t status =
      cudaMemcpy(device_ordinals, kInput.data(), kCount * sizeof(std::uint64_t),
                 cudaMemcpyHostToDevice);
  ASC_M7_CUDA_EQ(test, status, cudaSuccess);
  if (status == cudaSuccess) {
    SortCollidingOrdinals<<<1, 1>>>(device_ordinals);
    status = cudaGetLastError();
    ASC_M7_CUDA_EQ(test, status, cudaSuccess);
  }
  if (status == cudaSuccess) {
    status = cudaDeviceSynchronize();
    ASC_M7_CUDA_EQ(test, status, cudaSuccess);
  }

  std::array<std::uint64_t, kCount> actual{};
  if (status == cudaSuccess) {
    status = cudaMemcpy(actual.data(), device_ordinals,
                        kCount * sizeof(std::uint64_t), cudaMemcpyDeviceToHost);
    ASC_M7_CUDA_EQ(test, status, cudaSuccess);
  }
  if (status == cudaSuccess) {
    ASC_M7_CUDA_EQ(test, actual, kExpected);
  }
  ASC_M7_CUDA_EQ(test, cudaFree(device_ordinals), cudaSuccess);
  return test.Finish();
}
