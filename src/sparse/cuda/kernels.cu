#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "kernels_internal.h"

namespace asc::internal_sparse_cuda {
namespace {

constexpr unsigned int kThreadsPerBlock = 256;
constexpr unsigned int kMaximumBlocks = 65535;

unsigned int BlockCount(std::uint64_t count) {
  const std::uint64_t requested =
      (count + kThreadsPerBlock - 1) / kThreadsPerBlock;
  return static_cast<unsigned int>(
      std::min<std::uint64_t>(requested, kMaximumBlocks));
}

template <typename Element>
__global__ void CsrSpmvKernel(Element alpha, const nnz_t* outer_offsets,
                              const index_t* inner_indices,
                              const Element* values, extent_t rows,
                              const Element* input, stride_t input_stride,
                              Element beta, Element* output,
                              stride_t output_stride) {
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t row = first; row < static_cast<std::uint64_t>(rows);
       row += step) {
    Element sum = Element{};
    const nnz_t begin = outer_offsets[row];
    const nnz_t end = outer_offsets[row + 1U];
    for (nnz_t position = begin; position < end; ++position) {
      sum += values[position] * input[inner_indices[position] * input_stride];
    }
    Element result = alpha * sum;
    if (beta != Element{}) {
      result += beta * output[row * output_stride];
    }
    output[row * output_stride] = result;
  }
}

template <typename Element>
__device__ Element ReadOperand(OperandDescriptor operand,
                               std::uint64_t position) {
  if (operand.kind == OperandKind::kScalar) {
    return static_cast<Element>(operand.scalar);
  }
  return static_cast<const Element*>(operand.view.values)[position];
}

template <typename Element>
__global__ void SparsePointwiseKernel(int operation, OperandDescriptor left,
                                      OperandDescriptor right,
                                      SparseDescriptor destination) {
  auto* output = static_cast<Element*>(const_cast<void*>(destination.values));
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t position = first;
       position < static_cast<std::uint64_t>(destination.nonzeros);
       position += step) {
    const Element left_value = ReadOperand<Element>(left, position);
    Element value = left_value;
    switch (static_cast<PointwiseOperation>(operation)) {
      case PointwiseOperation::kCopy:
        value = left_value;
        break;
      case PointwiseOperation::kNegate:
        value = -left_value;
        break;
      case PointwiseOperation::kAdd:
        value = left_value + ReadOperand<Element>(right, position);
        break;
      case PointwiseOperation::kSubtract:
        value = left_value - ReadOperand<Element>(right, position);
        break;
      case PointwiseOperation::kMultiply:
        value = left_value * ReadOperand<Element>(right, position);
        break;
    }
    output[position] = value;
  }
}

Status LaunchStatus(const char* message) {
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(error, ErrorCode::kProvider, message);
  }
  return Status::Ok();
}

template <typename Element>
Status LaunchSpmvTyped(void* stream, double alpha, SparseDescriptor matrix,
                       VectorDescriptor input, double beta,
                       VectorDescriptor output) {
  if (matrix.extents[0] == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  CsrSpmvKernel<Element>
      <<<BlockCount(static_cast<std::uint64_t>(matrix.extents[0])),
         kThreadsPerBlock, 0, static_cast<cudaStream_t>(stream)>>>(
          static_cast<Element>(alpha),
          static_cast<const nnz_t*>(matrix.structure_first),
          static_cast<const index_t*>(matrix.structure_second),
          static_cast<const Element*>(matrix.values), matrix.extents[0],
          static_cast<const Element*>(input.data), input.stride,
          static_cast<Element>(beta),
          static_cast<Element*>(const_cast<void*>(output.data)), output.stride);
  return LaunchStatus("CUDA could not launch the strided CSR SpMV kernel");
}

template <typename Element>
Status LaunchPointwiseTyped(void* stream, PointwiseOperation operation,
                            OperandDescriptor left, OperandDescriptor right,
                            SparseDescriptor destination) {
  if (destination.nonzeros == 0) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  SparsePointwiseKernel<Element>
      <<<BlockCount(static_cast<std::uint64_t>(destination.nonzeros)),
         kThreadsPerBlock, 0, static_cast<cudaStream_t>(stream)>>>(
          static_cast<int>(operation), left, right, destination);
  return LaunchStatus("CUDA could not launch the sparse pointwise kernel");
}

}  // namespace

Status LaunchStridedCsrSpmv(void* stream, double alpha, SparseDescriptor matrix,
                            VectorDescriptor input, double beta,
                            VectorDescriptor output) {
  if (matrix.element_kind == ElementKind::kFloat) {
    return LaunchSpmvTyped<float>(stream, alpha, matrix, input, beta, output);
  }
  return LaunchSpmvTyped<double>(stream, alpha, matrix, input, beta, output);
}

Status LaunchSparsePointwise(void* stream, PointwiseOperation operation,
                             OperandDescriptor left, OperandDescriptor right,
                             SparseDescriptor destination) {
  if (destination.element_kind == ElementKind::kFloat) {
    return LaunchPointwiseTyped<float>(stream, operation, left, right,
                                       destination);
  }
  return LaunchPointwiseTyped<double>(stream, operation, left, right,
                                      destination);
}

}  // namespace asc::internal_sparse_cuda
