#include <cuda_runtime_api.h>
#include <driver_types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "asc/core/status.h"
#include "asc/random/engine.h"
#include "asc/random/providers/dense_cuda.h"
#include "dense_kernels_internal.h"

namespace asc::internal_random_dense_cuda {
namespace {

constexpr unsigned int kThreadsPerBlock = 256;
constexpr unsigned int kMaximumBlocks = 65535;

struct KernelView {
  void* data;
  std::size_t rank;
  std::int64_t extents[8];
  std::int64_t strides[8];
  std::uint64_t logical_size;
};

struct PhiloxBlock {
  std::uint32_t words[4];
};

__device__ std::uint32_t LowWord(std::uint64_t value) {
  return static_cast<std::uint32_t>(value);
}

__device__ std::uint32_t HighWord(std::uint64_t value) {
  return static_cast<std::uint32_t>(value >> 32U);
}

__device__ PhiloxBlock GenerateBlock(RandomStream random_stream,
                                     RandomSubsequence subsequence,
                                     std::uint64_t block) {
  constexpr std::uint64_t kFirstMultiplier = std::uint64_t{0xD2511F53};
  constexpr std::uint64_t kSecondMultiplier = std::uint64_t{0xCD9E8D57};
  constexpr std::uint32_t kFirstWeyl = std::uint32_t{0x9E3779B9};
  constexpr std::uint32_t kSecondWeyl = std::uint32_t{0xBB67AE85};
  PhiloxBlock counter{{LowWord(block), HighWord(block), LowWord(subsequence),
                       HighWord(subsequence)}};
  std::uint32_t key[2] = {LowWord(random_stream), HighWord(random_stream)};
  for (int round = 0; round < 10; ++round) {
    const std::uint64_t first_product =
        kFirstMultiplier * static_cast<std::uint64_t>(counter.words[0]);
    const std::uint64_t second_product =
        kSecondMultiplier * static_cast<std::uint64_t>(counter.words[2]);
    counter = {{
        HighWord(second_product) ^ counter.words[1] ^ key[0],
        LowWord(second_product),
        HighWord(first_product) ^ counter.words[3] ^ key[1],
        LowWord(first_product),
    }};
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

__device__ std::uint64_t PhysicalOffset(KernelView view,
                                        std::uint64_t logical_index) {
  std::uint64_t physical_offset = 0;
  for (std::size_t dimension = 0; dimension < view.rank; ++dimension) {
    const auto extent = static_cast<std::uint64_t>(view.extents[dimension]);
    const std::uint64_t coordinate = logical_index % extent;
    logical_index /= extent;
    physical_offset +=
        coordinate * static_cast<std::uint64_t>(view.strides[dimension]);
  }
  return physical_offset;
}

template <typename Element>
__global__ void DenseUniformKernel(KernelView destination,
                                   RandomStream random_stream,
                                   RandomSubsequence subsequence,
                                   RandomOffset offset) {
  constexpr std::uint64_t kWordsPerElement =
      sizeof(Element) == sizeof(float) ? 1U : 2U;
  auto* output = static_cast<Element*>(destination.data);
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first; position < destination.logical_size;
       position += step) {
    const RandomOffset word_offset = offset + position * kWordsPerElement;
    if constexpr (sizeof(Element) == sizeof(float)) {
      constexpr float kScale = 0x1.0p-24F;
      output[PhysicalOffset(destination, position)] =
          static_cast<float>(
              GenerateWord(random_stream, subsequence, word_offset) >> 8U) *
          kScale;
    } else {
      constexpr double kScale = 0x1.0p-53;
      const std::uint64_t bits =
          (static_cast<std::uint64_t>(
               GenerateWord(random_stream, subsequence, word_offset))
           << 32U) |
          GenerateWord(random_stream, subsequence, word_offset + 1U);
      output[PhysicalOffset(destination, position)] =
          static_cast<double>(bits >> 11U) * kScale;
    }
  }
}

KernelView ToKernelView(const ViewDescriptor& descriptor) {
  KernelView view{};
  view.data = descriptor.data;
  view.rank = descriptor.rank;
  view.logical_size = static_cast<std::uint64_t>(descriptor.logical_size);
  for (std::size_t dimension = 0; dimension < descriptor.rank; ++dimension) {
    view.extents[dimension] = descriptor.extents[dimension];
    view.strides[dimension] = descriptor.strides[dimension];
  }
  return view;
}

unsigned int BlockCount(std::uint64_t count) {
  const std::uint64_t requested =
      (count + kThreadsPerBlock - 1) / kThreadsPerBlock;
  return static_cast<unsigned int>(
      std::min<std::uint64_t>(requested, kMaximumBlocks));
}

template <typename Element>
Status LaunchTyped(void* stream, ViewDescriptor descriptor,
                   RandomStream random_stream, RandomSubsequence subsequence,
                   RandomOffset offset) {
  if (descriptor.logical_size == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  const KernelView destination = ToKernelView(descriptor);
  DenseUniformKernel<Element>
      <<<BlockCount(destination.logical_size), kThreadsPerBlock, 0,
         static_cast<cudaStream_t>(stream)>>>(destination, random_stream,
                                              subsequence, offset);
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kProvider,
        "CUDA could not launch the Dense Uniform01 kernel");
  }
  return Status::Ok();
}

}  // namespace

Status LaunchDenseUniform01(void* stream, ViewDescriptor destination,
                            RandomStream random_stream,
                            RandomSubsequence subsequence,
                            RandomOffset offset) {
  if (destination.element_kind == ElementKind::kFloat) {
    return LaunchTyped<float>(stream, destination, random_stream, subsequence,
                              offset);
  }
  return LaunchTyped<double>(stream, destination, random_stream, subsequence,
                             offset);
}

}  // namespace asc::internal_random_dense_cuda
