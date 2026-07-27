#include <cuda_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "../../core/cuda/provider_internal.h"
#include "kernels_internal.h"

namespace asc {
namespace internal_dense_cuda {
namespace {

Status KernelStatus(cudaError_t error, const char* operation) {
  if (error == cudaSuccess) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  return Status(ErrorCode::kProvider,
                std::string(operation) + ": " + cudaGetErrorString(error),
                "cuda", static_cast<std::int64_t>(error));
}

struct DevicePointwiseOperand {
  const void* data;
  stride_t strides[8];
  double scalar_value;
  bool is_scalar;
};

struct DevicePointwisePlan {
  PointwiseOperation operation;
  ScalarType scalar_type;
  std::uint8_t rank;
  extent_t shape[8];
  stride_t destination_strides[8];
  void* destination;
  DevicePointwiseOperand left;
  DevicePointwiseOperand right;
  extent_t logical_size;
  bool no_op;
};

struct DeviceLinalgOperand {
  const void* data;
  extent_t shape[2];
  stride_t strides[2];
};

struct DeviceLinalgPlan {
  LinalgOperation operation;
  ScalarType scalar_type;
  std::uint8_t rank;
  DeviceLinalgOperand left;
  DeviceLinalgOperand right;
  void* destination;
  extent_t destination_shape[2];
  stride_t destination_strides[2];
  double alpha;
  double beta;
  extent_t logical_size;
  bool no_op;
};

DevicePointwiseOperand MakeDeviceOperand(const PointwiseOperand& operand) {
  DevicePointwiseOperand result{};
  result.data = operand.data;
  result.scalar_value = operand.scalar_value;
  result.is_scalar = operand.is_scalar;
  for (std::size_t dimension = 0; dimension < 8; ++dimension) {
    result.strides[dimension] = operand.strides[dimension];
  }
  return result;
}

DevicePointwisePlan MakeDevicePlan(const PointwisePlan& plan) {
  DevicePointwisePlan result{};
  result.operation = plan.operation;
  result.scalar_type = plan.scalar_type;
  result.rank = plan.rank;
  result.destination = plan.destination;
  result.left = MakeDeviceOperand(plan.left);
  result.right = MakeDeviceOperand(plan.right);
  result.logical_size = plan.logical_size;
  result.no_op = plan.no_op;
  for (std::size_t dimension = 0; dimension < 8; ++dimension) {
    result.shape[dimension] = plan.shape[dimension];
    result.destination_strides[dimension] = plan.destination_strides[dimension];
  }
  return result;
}

DeviceLinalgOperand MakeDeviceOperand(const LinalgOperand& operand) {
  DeviceLinalgOperand result{};
  result.data = operand.data;
  for (std::size_t dimension = 0; dimension < 2; ++dimension) {
    result.shape[dimension] = operand.shape[dimension];
    result.strides[dimension] = operand.strides[dimension];
  }
  return result;
}

DeviceLinalgPlan MakeDevicePlan(const LinalgPlan& plan) {
  DeviceLinalgPlan result{};
  result.operation = plan.operation;
  result.scalar_type = plan.scalar_type;
  result.rank = plan.rank;
  result.left = MakeDeviceOperand(plan.left);
  result.right = MakeDeviceOperand(plan.right);
  result.destination = plan.destination;
  result.alpha = plan.alpha;
  result.beta = plan.beta;
  result.logical_size = plan.logical_size;
  result.no_op = plan.no_op;
  for (std::size_t dimension = 0; dimension < 2; ++dimension) {
    result.destination_shape[dimension] = plan.destination_shape[dimension];
    result.destination_strides[dimension] = plan.destination_strides[dimension];
  }
  return result;
}

template <typename Scalar>
__device__ Scalar ReadOperand(const DevicePointwiseOperand& operand,
                              const index_t (&coordinates)[8],
                              std::uint8_t rank) {
  if (operand.is_scalar) {
    return static_cast<Scalar>(operand.scalar_value);
  }
  extent_t offset = 0;
  for (std::uint8_t dimension = 0; dimension < rank; ++dimension) {
    offset += coordinates[dimension] * operand.strides[dimension];
  }
  return static_cast<const Scalar*>(operand.data)[offset];
}

template <typename Scalar>
__global__ void PointwiseKernel(DevicePointwisePlan plan) {
  const extent_t first_ordinal =
      static_cast<extent_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const extent_t ordinal_stride = static_cast<extent_t>(blockDim.x) * gridDim.x;
  extent_t ordinal = first_ordinal;
  while (ordinal < plan.logical_size) {
    index_t coordinates[8]{};
    extent_t remaining = ordinal;
    extent_t destination_offset = 0;
    for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
      const extent_t coordinate = remaining % plan.shape[dimension];
      remaining /= plan.shape[dimension];
      coordinates[dimension] = coordinate;
      destination_offset += coordinate * plan.destination_strides[dimension];
    }

    const Scalar left = ReadOperand<Scalar>(plan.left, coordinates, plan.rank);
    Scalar value = left;
    switch (plan.operation) {
      case PointwiseOperation::kCopy:
      case PointwiseOperation::kFill:
        break;
      case PointwiseOperation::kNegate:
        value = -left;
        break;
      case PointwiseOperation::kAdd:
        value = left + ReadOperand<Scalar>(plan.right, coordinates, plan.rank);
        break;
      case PointwiseOperation::kSubtract:
        value = left - ReadOperand<Scalar>(plan.right, coordinates, plan.rank);
        break;
      case PointwiseOperation::kMultiply:
        value = left * ReadOperand<Scalar>(plan.right, coordinates, plan.rank);
        break;
    }
    static_cast<Scalar*>(plan.destination)[destination_offset] = value;
    if (!AdvanceCudaGridStrideOrdinal(plan.logical_size, ordinal_stride,
                                      ordinal)) {
      break;
    }
  }
}

template <typename Scalar>
__global__ void BasicLinalgKernel(DeviceLinalgPlan plan) {
  const extent_t first_ordinal =
      static_cast<extent_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const extent_t ordinal_stride = static_cast<extent_t>(blockDim.x) * gridDim.x;
  extent_t ordinal = first_ordinal;
  while (ordinal < plan.logical_size) {
    extent_t first = ordinal;
    extent_t second = 0;
    if (plan.rank == 2) {
      first = ordinal % plan.destination_shape[0];
      second = ordinal / plan.destination_shape[0];
    }
    const extent_t destination_offset = first * plan.destination_strides[0] +
                                        second * plan.destination_strides[1];
    const extent_t source_offset =
        first * plan.left.strides[0] + second * plan.left.strides[1];
    Scalar* destination = static_cast<Scalar*>(plan.destination);
    const Scalar alpha = static_cast<Scalar>(plan.alpha);
    switch (plan.operation) {
      case LinalgOperation::kCopy:
        destination[destination_offset] =
            static_cast<const Scalar*>(plan.left.data)[source_offset];
        break;
      case LinalgOperation::kScal:
        destination[destination_offset] *= alpha;
        break;
      case LinalgOperation::kAxpy:
        destination[destination_offset] =
            alpha * static_cast<const Scalar*>(plan.left.data)[source_offset] +
            destination[destination_offset];
        break;
      case LinalgOperation::kScaleOrZero:
        if (alpha == static_cast<Scalar>(0)) {
          destination[destination_offset] = static_cast<Scalar>(0);
        } else {
          destination[destination_offset] *= alpha;
        }
        break;
      case LinalgOperation::kGemv:
      case LinalgOperation::kGemm:
        break;
    }
    if (!AdvanceCudaGridStrideOrdinal(plan.logical_size, ordinal_stride,
                                      ordinal)) {
      break;
    }
  }
}

template <typename DevicePlan, typename Kernel>
Status Launch(const ExecutionContext& execution_context, const DevicePlan& plan,
              Kernel kernel) {
  if (plan.logical_size == 0 || plan.no_op) {
    return Status::Ok();
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(execution_context);
  if (!guard.ok()) {
    return guard.status();
  }
  auto stream = internal_core_cuda::CudaStreamHandle(execution_context);
  if (!stream.ok()) {
    return stream.status();
  }
  // Consume a prior operation's sticky thread-local error before this
  // submission. Only the resetting post-launch check is attributed here.
  static_cast<void>(cudaGetLastError());
  const unsigned int block_count = CudaLaunchBlockCount(plan.logical_size);
  kernel<<<block_count, kCudaKernelBlockSize, 0,
           reinterpret_cast<cudaStream_t>(*stream)>>>(plan);
  const cudaError_t launch_error = cudaGetLastError();
  if (launch_error != cudaSuccess) {
    // A CUDA Runtime launch check may surface an earlier asynchronous failure
    // after this kernel was accepted. Drain this stream before returning
    // without a completion event.
    static_cast<void>(
        cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(*stream)));
  }
  return KernelStatus(launch_error, "CUDA kernel launch");
}

}  // namespace

Status LaunchPointwiseKernel(const ExecutionContext& execution_context,
                             const PointwisePlan& plan) {
  const DevicePointwisePlan device_plan = MakeDevicePlan(plan);
  if (plan.scalar_type == ScalarType::kFloat) {
    return Launch(execution_context, device_plan, PointwiseKernel<float>);
  }
  return Launch(execution_context, device_plan, PointwiseKernel<double>);
}

Status LaunchBasicLinalgKernel(const ExecutionContext& execution_context,
                               const LinalgPlan& plan) {
  const DeviceLinalgPlan device_plan = MakeDevicePlan(plan);
  if (plan.scalar_type == ScalarType::kFloat) {
    return Launch(execution_context, device_plan, BasicLinalgKernel<float>);
  }
  return Launch(execution_context, device_plan, BasicLinalgKernel<double>);
}

}  // namespace internal_dense_cuda
}  // namespace asc
