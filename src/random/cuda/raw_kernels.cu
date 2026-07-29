#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "raw_kernels_internal.h"

namespace asc::internal_random_cuda {
namespace {

constexpr unsigned int kThreadsPerBlock = 256;
constexpr unsigned int kMaximumBlocks = 65535;

__device__ std::uint32_t LowWord(std::uint64_t value) {
  return static_cast<std::uint32_t>(value);
}

__device__ std::uint32_t HighWord(std::uint64_t value) {
  return static_cast<std::uint32_t>(value >> 32U);
}

struct PhiloxBlock {
  std::uint32_t words[4];
};

__device__ PhiloxBlock GenerateBlock(RandomStream random_stream,
                                     RandomSubsequence subsequence,
                                     std::uint64_t block) {
  constexpr std::uint64_t kFirstMultiplier = UINT64_C(0xD2511F53);
  constexpr std::uint64_t kSecondMultiplier = UINT64_C(0xCD9E8D57);
  constexpr std::uint32_t kFirstWeyl = UINT32_C(0x9E3779B9);
  constexpr std::uint32_t kSecondWeyl = UINT32_C(0xBB67AE85);

  PhiloxBlock counter{{LowWord(block), HighWord(block), LowWord(subsequence),
                       HighWord(subsequence)}};
  std::uint32_t key[2] = {LowWord(random_stream), HighWord(random_stream)};
  for (int round = 0; round < 10; ++round) {
    const std::uint64_t first_product =
        kFirstMultiplier * static_cast<std::uint64_t>(counter.words[0]);
    const std::uint64_t second_product =
        kSecondMultiplier * static_cast<std::uint64_t>(counter.words[2]);
    const PhiloxBlock next{{
        HighWord(second_product) ^ counter.words[1] ^ key[0],
        LowWord(second_product),
        HighWord(first_product) ^ counter.words[3] ^ key[1],
        LowWord(first_product),
    }};
    counter = next;
    if (round != 9) {
      key[0] += kFirstWeyl;
      key[1] += kSecondWeyl;
    }
  }
  return counter;
}

__device__ std::uint32_t GenerateWord(RandomStream random_stream,
                                      RandomSubsequence subsequence,
                                      RandomOffset offset) {
  const PhiloxBlock block =
      GenerateBlock(random_stream, subsequence, offset / 4U);
  return block.words[offset % 4U];
}

__global__ void PhiloxWordsKernel(std::uint32_t* destination,
                                  std::uint64_t word_count,
                                  RandomStream random_stream,
                                  RandomSubsequence subsequence,
                                  RandomOffset offset) {
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first; position < word_count;
       position += step) {
    destination[position] =
        GenerateWord(random_stream, subsequence, offset + position);
  }
}

unsigned int BlockCount(std::uint64_t count) {
  const std::uint64_t requested =
      (count + kThreadsPerBlock - 1) / kThreadsPerBlock;
  return static_cast<unsigned int>(
      std::min<std::uint64_t>(requested, kMaximumBlocks));
}

}  // namespace

Status LaunchPhiloxWords(void* stream, std::uint32_t* destination,
                         std::uint64_t word_count, RandomStream random_stream,
                         RandomSubsequence subsequence, RandomOffset offset) {
  if (word_count == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  PhiloxWordsKernel<<<BlockCount(word_count), kThreadsPerBlock, 0,
                      static_cast<cudaStream_t>(stream)>>>(
      destination, word_count, random_stream, subsequence, offset);
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kProvider,
        "CUDA could not launch the Philox word kernel");
  }
  return Status::Ok();
}

}  // namespace asc::internal_random_cuda
