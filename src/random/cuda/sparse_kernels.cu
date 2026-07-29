#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "sparse_kernels_internal.h"

namespace asc::internal_random_sparse_cuda {
namespace {

constexpr unsigned int kThreadsPerBlock = 256;
constexpr unsigned int kMaximumBlocks = 65535;

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

__device__ std::uint64_t Priority(RandomStream random_stream,
                                  RandomSubsequence subsequence,
                                  RandomOffset offset) {
  return (static_cast<std::uint64_t>(
              GenerateWord(random_stream, subsequence, offset))
          << 32U) |
         GenerateWord(random_stream, subsequence, offset + 1U);
}

__global__ void SelectCanonicalOrdinals(SparseDescriptor destination,
                                        RandomStream structure_stream,
                                        RandomSubsequence structure_subsequence,
                                        RandomOffset structure_offset) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  bool has_previous = false;
  std::uint64_t previous_priority = 0;
  std::uint64_t previous_ordinal = 0;
  for (std::uint64_t selected = 0; selected < destination.exact_count;
       ++selected) {
    bool found = false;
    std::uint64_t best_priority = 0;
    std::uint64_t best_ordinal = 0;
    for (std::uint64_t ordinal = 0; ordinal < destination.logical_size;
         ++ordinal) {
      const std::uint64_t priority =
          Priority(structure_stream, structure_subsequence,
                   structure_offset + 2U * ordinal);
      if (has_previous &&
          !PriorityOrdinalLess(previous_priority, previous_ordinal, priority,
                               ordinal)) {
        continue;
      }
      if (!found ||
          PriorityOrdinalLess(priority, ordinal, best_priority, best_ordinal)) {
        found = true;
        best_priority = priority;
        best_ordinal = ordinal;
      }
    }
    std::uint64_t insertion = selected;
    while (
        insertion > 0 &&
        best_ordinal <
            static_cast<std::uint64_t>(
                destination.coordinates[(insertion - 1U) * destination.rank])) {
      destination.coordinates[insertion * destination.rank] =
          destination.coordinates[(insertion - 1U) * destination.rank];
      --insertion;
    }
    destination.coordinates[insertion * destination.rank] =
        static_cast<index_t>(best_ordinal);
    has_previous = true;
    previous_priority = best_priority;
    previous_ordinal = best_ordinal;
  }
}

__global__ void DecodeCoordinateDimension(SparseDescriptor destination,
                                          std::size_t dimension,
                                          std::uint64_t divisor,
                                          std::uint64_t extent) {
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first; position < destination.exact_count;
       position += step) {
    const std::uint64_t ordinal = static_cast<std::uint64_t>(
        destination.coordinates[position * destination.rank]);
    destination.coordinates[position * destination.rank + dimension] =
        static_cast<index_t>((ordinal / divisor) % extent);
  }
}

template <typename Element>
__global__ void FillValues(SparseDescriptor destination,
                           RandomStream value_stream,
                           RandomSubsequence value_subsequence,
                           RandomOffset value_offset) {
  constexpr std::uint64_t kWordsPerElement =
      sizeof(Element) == sizeof(float) ? 1U : 2U;
  auto* output = static_cast<Element*>(destination.values);
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first; position < destination.exact_count;
       position += step) {
    const RandomOffset word_offset = value_offset + position * kWordsPerElement;
    if constexpr (sizeof(Element) == sizeof(float)) {
      constexpr float kScale = 0x1.0p-24F;
      output[position] =
          static_cast<float>(
              GenerateWord(value_stream, value_subsequence, word_offset) >>
              8U) *
          kScale;
    } else {
      constexpr double kScale = 0x1.0p-53;
      const std::uint64_t bits =
          (static_cast<std::uint64_t>(
               GenerateWord(value_stream, value_subsequence, word_offset))
           << 32U) |
          GenerateWord(value_stream, value_subsequence, word_offset + 1U);
      output[position] = static_cast<double>(bits >> 11U) * kScale;
    }
  }
}

unsigned int BlockCount(std::uint64_t count) {
  const std::uint64_t requested =
      (count + kThreadsPerBlock - 1) / kThreadsPerBlock;
  return static_cast<unsigned int>(
      std::min<std::uint64_t>(requested, kMaximumBlocks));
}

Status LaunchStatus(const char* message) {
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kProvider, message);
  }
  return Status::Ok();
}

}  // namespace

Status LaunchSparseUniform01(void* stream, SparseDescriptor destination,
                             std::span<const extent_t> extents,
                             RandomStream structure_stream,
                             RandomSubsequence structure_subsequence,
                             RandomOffset structure_offset,
                             RandomStream value_stream,
                             RandomSubsequence value_subsequence,
                             RandomOffset value_offset) {
  if (destination.exact_count == 0) {
    return Status::Ok();
  }
  const auto cuda_stream = static_cast<cudaStream_t>(stream);
  bool submitted = false;
  static_cast<void>(cudaGetLastError());
  if (destination.rank != 0) {
    SelectCanonicalOrdinals<<<1, 1, 0, cuda_stream>>>(
        destination, structure_stream, structure_subsequence, structure_offset);
    Status coordinate_status =
        LaunchStatus("CUDA could not launch sparse coordinate selection");
    if (!coordinate_status.ok()) {
      return coordinate_status;
    }
    submitted = true;

    std::uint64_t divisor = 1;
    for (std::size_t reverse = destination.rank; reverse > 0; --reverse) {
      const std::size_t dimension = reverse - 1;
      DecodeCoordinateDimension<<<BlockCount(destination.exact_count),
                                  kThreadsPerBlock, 0, cuda_stream>>>(
          destination, dimension, divisor,
          static_cast<std::uint64_t>(extents[dimension]));
      Status decode_status =
          LaunchStatus("CUDA could not launch sparse coordinate decoding");
      if (!decode_status.ok()) {
        static_cast<void>(cudaStreamSynchronize(cuda_stream));
        return decode_status;
      }
      divisor *= static_cast<std::uint64_t>(extents[dimension]);
    }
  }
  if (destination.element_kind == ElementKind::kFloat) {
    FillValues<float><<<BlockCount(destination.exact_count), kThreadsPerBlock,
                        0, cuda_stream>>>(destination, value_stream,
                                          value_subsequence, value_offset);
  } else {
    FillValues<double><<<BlockCount(destination.exact_count), kThreadsPerBlock,
                         0, cuda_stream>>>(destination, value_stream,
                                           value_subsequence, value_offset);
  }
  Status value_status =
      LaunchStatus("CUDA could not launch sparse Uniform01 generation");
  if (!value_status.ok() && submitted) {
    static_cast<void>(cudaStreamSynchronize(cuda_stream));
  }
  return value_status;
}

}  // namespace asc::internal_random_sparse_cuda
