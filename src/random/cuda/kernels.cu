#include <cuda_runtime_api.h>

#include <cstdint>
#include <string>

#include "../../core/cuda/provider_internal.h"
#include "kernels_internal.h"

namespace asc {
namespace internal_random_cuda {
namespace {

constexpr unsigned int kBlockSize = 256;
constexpr std::uint64_t kMaximumBlockCount = 65535;
constexpr std::uint32_t kMultiplier0 = 0xd2511f53U;
constexpr std::uint32_t kMultiplier1 = 0xcd9e8d57U;
constexpr std::uint32_t kWeyl0 = 0x9e3779b9U;
constexpr std::uint32_t kWeyl1 = 0xbb67ae85U;

__device__ void PhiloxRound(std::uint32_t (&counter)[4],
                            const std::uint32_t (&key)[2]) {
  const std::uint64_t product0 =
      static_cast<std::uint64_t>(kMultiplier0) * counter[0];
  const std::uint64_t product1 =
      static_cast<std::uint64_t>(kMultiplier1) * counter[2];
  const std::uint32_t next0 =
      static_cast<std::uint32_t>(product1 >> 32U) ^ counter[1] ^ key[0];
  const std::uint32_t next1 = static_cast<std::uint32_t>(product1);
  const std::uint32_t next2 =
      static_cast<std::uint32_t>(product0 >> 32U) ^ counter[3] ^ key[1];
  const std::uint32_t next3 = static_cast<std::uint32_t>(product0);
  counter[0] = next0;
  counter[1] = next1;
  counter[2] = next2;
  counter[3] = next3;
}

__device__ std::uint32_t PhiloxWord(RandomStream stream,
                                    RandomSubsequence subsequence,
                                    RandomOffset offset) {
  const std::uint64_t block = offset / 4U;
  std::uint32_t counter[4]{
      static_cast<std::uint32_t>(block),
      static_cast<std::uint32_t>(block >> 32U),
      static_cast<std::uint32_t>(subsequence),
      static_cast<std::uint32_t>(subsequence >> 32U),
  };
  std::uint32_t key[2]{
      static_cast<std::uint32_t>(stream),
      static_cast<std::uint32_t>(stream >> 32U),
  };
  for (int round = 0; round < 10; ++round) {
    PhiloxRound(counter, key);
    if (round != 9) {
      key[0] += kWeyl0;
      key[1] += kWeyl1;
    }
  }
  return counter[offset % 4U];
}

__global__ void RawPhiloxKernel(RawPhiloxPlan plan) {
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t stride =
      static_cast<std::uint64_t>(blockDim.x) * gridDim.x;
  for (std::uint64_t ordinal = first; ordinal < plan.word_count;
       ordinal += stride) {
    plan.destination[ordinal] =
        PhiloxWord(plan.stream, plan.subsequence, plan.offset + ordinal);
  }
}

Status KernelStatus(cudaError_t error) {
  if (error == cudaSuccess) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  return Status(
      ErrorCode::kProvider,
      std::string("CUDA random kernel launch: ") + cudaGetErrorString(error),
      "cuda", static_cast<std::int64_t>(error));
}

}  // namespace

Status LaunchRawPhiloxKernel(const ExecutionContext& context,
                             const RawPhiloxPlan& plan) {
  if (plan.word_count == 0) {
    return Status::Ok();
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(context);
  if (!guard.ok()) {
    return guard.status();
  }
  auto stream = internal_core_cuda::CudaStreamHandle(context);
  if (!stream.ok()) {
    return stream.status();
  }
  const auto required = (plan.word_count + kBlockSize - 1U) / kBlockSize;
  const auto block_count = static_cast<unsigned int>(
      required < kMaximumBlockCount ? required : kMaximumBlockCount);
  static_cast<void>(cudaGetLastError());
  RawPhiloxKernel<<<block_count, kBlockSize, 0,
                    reinterpret_cast<cudaStream_t>(*stream)>>>(plan);
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    static_cast<void>(
        cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(*stream)));
  }
  return KernelStatus(error);
}

}  // namespace internal_random_cuda
}  // namespace asc
