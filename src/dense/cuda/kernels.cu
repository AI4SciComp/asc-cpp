#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "../../core/cuda/cuda_internal.h"
#include "kernels_internal.h"

namespace asc::internal_dense_cuda {
namespace {

constexpr unsigned int kThreadsPerBlock = 256;
constexpr unsigned int kMaximumBlocks = 65535;

struct KernelView {
  const void* data;
  std::size_t rank;
  std::int64_t extents[8];
  std::int64_t strides[8];
  std::uint64_t logical_size;
};

struct KernelOperand {
  int kind;
  KernelView view;
  double scalar;
};

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

KernelOperand ToKernelOperand(const OperandDescriptor& descriptor) {
  KernelOperand operand{};
  operand.kind = static_cast<int>(descriptor.kind);
  operand.view = ToKernelView(descriptor.view);
  operand.scalar = descriptor.scalar;
  return operand;
}

unsigned int BlockCount(std::uint64_t element_count) {
  const std::uint64_t requested =
      (element_count + kThreadsPerBlock - 1) / kThreadsPerBlock;
  return static_cast<unsigned int>(
      std::min<std::uint64_t>(requested, kMaximumBlocks));
}

__device__ std::uint64_t Offset(KernelView view, std::uint64_t logical_index) {
  std::uint64_t offset = 0;
  for (std::size_t dimension = 0; dimension < view.rank; ++dimension) {
    const std::uint64_t extent =
        static_cast<std::uint64_t>(view.extents[dimension]);
    const std::uint64_t coordinate = logical_index % extent;
    logical_index /= extent;
    offset += coordinate * static_cast<std::uint64_t>(view.strides[dimension]);
  }
  return offset;
}

template <typename Element>
__device__ Element ReadOperand(KernelOperand operand,
                               std::uint64_t logical_index) {
  if (operand.kind == static_cast<int>(OperandKind::kScalar)) {
    return static_cast<Element>(operand.scalar);
  }
  const auto* data = static_cast<const Element*>(operand.view.data);
  return data[Offset(operand.view, logical_index)];
}

template <typename Element>
__global__ void PointwiseKernel(int operation, KernelOperand left,
                                KernelOperand right, KernelView destination) {
  auto* output = static_cast<Element*>(const_cast<void*>(destination.data));
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t logical_index = first;
       logical_index < destination.logical_size; logical_index += step) {
    const Element left_value = ReadOperand<Element>(left, logical_index);
    Element value = left_value;
    switch (static_cast<PointwiseOperation>(operation)) {
      case PointwiseOperation::kCopy:
      case PointwiseOperation::kFill:
        value = left_value;
        break;
      case PointwiseOperation::kNegate:
        value = -left_value;
        break;
      case PointwiseOperation::kAdd:
        value = left_value + ReadOperand<Element>(right, logical_index);
        break;
      case PointwiseOperation::kSubtract:
        value = left_value - ReadOperand<Element>(right, logical_index);
        break;
      case PointwiseOperation::kMultiply:
        value = left_value * ReadOperand<Element>(right, logical_index);
        break;
    }
    output[Offset(destination, logical_index)] = value;
  }
}

template <typename Element>
__global__ void ScalKernel(Element alpha, KernelView destination) {
  auto* output = static_cast<Element*>(const_cast<void*>(destination.data));
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t logical_index = first;
       logical_index < destination.logical_size; logical_index += step) {
    const std::uint64_t offset = Offset(destination, logical_index);
    output[offset] = alpha * output[offset];
  }
}

template <typename Element>
__global__ void AxpyKernel(Element alpha, KernelView source,
                           KernelView destination) {
  const auto* input = static_cast<const Element*>(source.data);
  auto* output = static_cast<Element*>(const_cast<void*>(destination.data));
  const std::uint64_t first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const std::uint64_t step = static_cast<std::uint64_t>(gridDim.x) * blockDim.x;
  for (std::uint64_t logical_index = first;
       logical_index < destination.logical_size; logical_index += step) {
    const std::uint64_t source_offset = Offset(source, logical_index);
    const std::uint64_t destination_offset = Offset(destination, logical_index);
    output[destination_offset] =
        alpha * input[source_offset] + output[destination_offset];
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
Status LaunchPointwiseTyped(cudaStream_t stream, PointwiseOperation operation,
                            OperandDescriptor left, OperandDescriptor right,
                            ViewDescriptor destination) {
  if (destination.logical_size == 0) {
    return Status::Ok();
  }
  const KernelView output = ToKernelView(destination);
  PointwiseKernel<Element>
      <<<BlockCount(output.logical_size), kThreadsPerBlock, 0, stream>>>(
          static_cast<int>(operation), ToKernelOperand(left),
          ToKernelOperand(right), output);
  return LaunchStatus("CUDA could not launch a pointwise Dense kernel");
}

template <typename Element>
Status LaunchScalTyped(cudaStream_t stream, double alpha,
                       ViewDescriptor destination) {
  if (destination.logical_size == 0) {
    return Status::Ok();
  }
  const KernelView output = ToKernelView(destination);
  ScalKernel<Element>
      <<<BlockCount(output.logical_size), kThreadsPerBlock, 0, stream>>>(
          static_cast<Element>(alpha), output);
  return LaunchStatus("CUDA could not launch a Dense scaling kernel");
}

template <typename Element>
Status LaunchAxpyTyped(cudaStream_t stream, double alpha, ViewDescriptor source,
                       ViewDescriptor destination) {
  if (destination.logical_size == 0) {
    return Status::Ok();
  }
  const KernelView input = ToKernelView(source);
  const KernelView output = ToKernelView(destination);
  AxpyKernel<Element>
      <<<BlockCount(output.logical_size), kThreadsPerBlock, 0, stream>>>(
          static_cast<Element>(alpha), input, output);
  return LaunchStatus("CUDA could not launch a Dense Axpy kernel");
}

}  // namespace

Status LaunchPointwiseKernel(void* stream, PointwiseOperation operation,
                             OperandDescriptor left, OperandDescriptor right,
                             ViewDescriptor destination) {
  if (destination.element_kind == ElementKind::kFloat) {
    return LaunchPointwiseTyped<float>(static_cast<cudaStream_t>(stream),
                                       operation, left, right, destination);
  }
  return LaunchPointwiseTyped<double>(static_cast<cudaStream_t>(stream),
                                      operation, left, right, destination);
}

Status LaunchCopyKernel(void* stream, ViewDescriptor source,
                        ViewDescriptor destination) {
  OperandDescriptor operand;
  operand.kind = OperandKind::kView;
  operand.view = source;
  return LaunchPointwiseKernel(stream, PointwiseOperation::kCopy, operand, {},
                               destination);
}

Status LaunchScalKernel(void* stream, double alpha,
                        ViewDescriptor destination) {
  if (destination.element_kind == ElementKind::kFloat) {
    return LaunchScalTyped<float>(static_cast<cudaStream_t>(stream), alpha,
                                  destination);
  }
  return LaunchScalTyped<double>(static_cast<cudaStream_t>(stream), alpha,
                                 destination);
}

Status LaunchAxpyKernel(void* stream, double alpha, ViewDescriptor source,
                        ViewDescriptor destination) {
  if (destination.element_kind == ElementKind::kFloat) {
    return LaunchAxpyTyped<float>(static_cast<cudaStream_t>(stream), alpha,
                                  source, destination);
  }
  return LaunchAxpyTyped<double>(static_cast<cudaStream_t>(stream), alpha,
                                 source, destination);
}

Status LaunchFillKernel(void* stream, double value,
                        ViewDescriptor destination) {
  OperandDescriptor operand;
  operand.kind = OperandKind::kScalar;
  operand.view.element_kind = destination.element_kind;
  operand.scalar = value;
  return LaunchPointwiseKernel(stream, PointwiseOperation::kFill, operand, {},
                               destination);
}

}  // namespace asc::internal_dense_cuda
