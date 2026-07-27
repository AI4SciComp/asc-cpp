#include <cuda_runtime_api.h>

#include <cstdint>
#include <string>

#include "../../core/cuda/provider_internal.h"
#include "sparse_kernels_internal.h"

namespace asc {
namespace internal_random_sparse_cuda {
namespace {

constexpr std::uint32_t kMultiplier0 = 0xd2511f53U;
constexpr std::uint32_t kMultiplier1 = 0xcd9e8d57U;
constexpr std::uint32_t kWeyl0 = 0x9e3779b9U;
constexpr std::uint32_t kWeyl1 = 0xbb67ae85U;

struct Priority {
  std::uint64_t value;
  std::uint64_t ordinal;
};

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
    if (round != 9) {
      key[0] += kWeyl0;
      key[1] += kWeyl1;
    }
  }
  return counter[offset % 4U];
}

__device__ Priority StructurePriority(const SparseUniformPlan& plan,
                                      std::uint64_t ordinal) {
  const RandomOffset address = plan.structure_offset + 2U * ordinal;
  const auto high = static_cast<std::uint64_t>(
      PhiloxWord(plan.structure_stream, plan.structure_subsequence, address));
  const auto low = static_cast<std::uint64_t>(PhiloxWord(
      plan.structure_stream, plan.structure_subsequence, address + 1U));
  return Priority{(high << 32U) | low, ordinal};
}

__device__ bool PriorityLess(Priority left, Priority right) {
  return left.value < right.value ||
         (left.value == right.value && left.ordinal < right.ordinal);
}

__device__ void WriteCoordinate(const SparseUniformPlan& plan, nnz_t position,
                                std::uint64_t ordinal) {
  for (std::uint8_t reverse = plan.rank; reverse > 0; --reverse) {
    const std::uint8_t dimension = reverse - 1;
    const auto extent = static_cast<std::uint64_t>(plan.shape[dimension]);
    plan.coordinates[position * plan.rank + dimension] =
        static_cast<index_t>(ordinal % extent);
    ordinal /= extent;
  }
}

__device__ void SortCoordinates(const SparseUniformPlan& plan) {
  for (nnz_t position = 1; position < plan.exact_count; ++position) {
    index_t coordinate[8]{};
    for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
      coordinate[dimension] =
          plan.coordinates[position * plan.rank + dimension];
    }
    nnz_t insertion = position;
    while (insertion > 0) {
      bool greater = false;
      bool different = false;
      for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
        const index_t previous =
            plan.coordinates[(insertion - 1) * plan.rank + dimension];
        if (previous != coordinate[dimension]) {
          greater = previous > coordinate[dimension];
          different = true;
          break;
        }
      }
      if (!different || !greater) {
        break;
      }
      for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
        plan.coordinates[insertion * plan.rank + dimension] =
            plan.coordinates[(insertion - 1) * plan.rank + dimension];
      }
      --insertion;
    }
    for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
      plan.coordinates[insertion * plan.rank + dimension] =
          coordinate[dimension];
    }
  }
}

template <typename Element>
__device__ void WriteValues(const SparseUniformPlan& plan);

template <>
__device__ void WriteValues<float>(const SparseUniformPlan& plan) {
  for (nnz_t position = 0; position < plan.exact_count; ++position) {
    const auto word =
        PhiloxWord(plan.value_stream, plan.value_subsequence,
                   plan.value_offset + static_cast<RandomOffset>(position));
    static_cast<float*>(plan.values)[position] =
        static_cast<float>(word >> 8U) * 0x1p-24F;
  }
}

template <>
__device__ void WriteValues<double>(const SparseUniformPlan& plan) {
  for (nnz_t position = 0; position < plan.exact_count; ++position) {
    const RandomOffset address =
        plan.value_offset + 2U * static_cast<RandomOffset>(position);
    const std::uint64_t bits =
        (static_cast<std::uint64_t>(
             PhiloxWord(plan.value_stream, plan.value_subsequence, address))
         << 32U) |
        PhiloxWord(plan.value_stream, plan.value_subsequence, address + 1U);
    static_cast<double*>(plan.values)[position] =
        static_cast<double>(bits >> 11U) * 0x1p-53;
  }
}

__global__ void SparseUniformKernel(SparseUniformPlan plan) {
  if (blockIdx.x != 0 || threadIdx.x != 0) {
    return;
  }
  Priority previous{};
  bool has_previous = false;
  for (nnz_t selected = 0; selected < plan.exact_count; ++selected) {
    Priority best{};
    bool has_best = false;
    for (std::uint64_t ordinal = 0;
         ordinal < static_cast<std::uint64_t>(plan.logical_size); ++ordinal) {
      const Priority candidate = StructurePriority(plan, ordinal);
      if (has_previous && !PriorityLess(previous, candidate)) {
        continue;
      }
      if (!has_best || PriorityLess(candidate, best)) {
        best = candidate;
        has_best = true;
      }
    }
    if (has_best) {
      WriteCoordinate(plan, selected, best.ordinal);
      previous = best;
      has_previous = true;
    }
  }
  SortCoordinates(plan);
  if (plan.scalar_type == ScalarType::kFloat) {
    WriteValues<float>(plan);
  } else {
    WriteValues<double>(plan);
  }
}

Status KernelStatus(cudaError_t error) {
  if (error == cudaSuccess) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  return Status(ErrorCode::kProvider,
                std::string("CUDA sparse random kernel launch: ") +
                    cudaGetErrorString(error),
                "cuda", static_cast<std::int64_t>(error));
}

}  // namespace

Status LaunchSparseUniform01Kernel(const ExecutionContext& context,
                                   const SparseUniformPlan& plan) {
  if (plan.exact_count == 0) {
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
  static_cast<void>(cudaGetLastError());
  SparseUniformKernel<<<1, 1, 0, reinterpret_cast<cudaStream_t>(*stream)>>>(
      plan);
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    static_cast<void>(
        cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(*stream)));
  }
  return KernelStatus(error);
}

}  // namespace internal_random_sparse_cuda
}  // namespace asc
