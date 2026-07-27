#include <cuda_runtime_api.h>

#include <cstdint>
#include <string>

#include "../../core/cuda/provider_internal.h"
#include "kernels_internal.h"

namespace asc {
namespace internal_sparse_cuda {
namespace {

constexpr unsigned int kBlockSize = 256;
constexpr std::uint64_t kMaximumBlockCount = 65535;

template <typename Scalar>
__device__ Scalar ReadOperand(const PointwiseOperand& operand, nnz_t position) {
  if (operand.is_scalar) {
    return static_cast<Scalar>(operand.scalar);
  }
  return static_cast<const Scalar*>(operand.values)[position];
}

template <typename Scalar>
__global__ void SparsePointwiseKernel(PointwisePlan plan) {
  const auto first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const auto stride = static_cast<std::uint64_t>(blockDim.x) * gridDim.x;
  for (std::uint64_t position = first;
       position < static_cast<std::uint64_t>(plan.nnz); position += stride) {
    const Scalar left =
        ReadOperand<Scalar>(plan.left, static_cast<nnz_t>(position));
    Scalar value = left;
    switch (plan.operation) {
      case PointwiseOperation::kCopy:
        break;
      case PointwiseOperation::kNegate:
        value = -left;
        break;
      case PointwiseOperation::kAdd:
        value = left +
                ReadOperand<Scalar>(plan.right, static_cast<nnz_t>(position));
        break;
      case PointwiseOperation::kSubtract:
        value = left -
                ReadOperand<Scalar>(plan.right, static_cast<nnz_t>(position));
        break;
      case PointwiseOperation::kMultiply:
        value = left *
                ReadOperand<Scalar>(plan.right, static_cast<nnz_t>(position));
        break;
    }
    static_cast<Scalar*>(plan.destination)[position] = value;
  }
}

template <typename Scalar>
__global__ void StridedSpmvKernel(SpmvPlan plan) {
  const auto first =
      static_cast<std::uint64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  const auto stride = static_cast<std::uint64_t>(blockDim.x) * gridDim.x;
  for (std::uint64_t row = first; row < static_cast<std::uint64_t>(plan.rows);
       row += stride) {
    Scalar sum = 0;
    const nnz_t begin = plan.outer_offsets[row];
    const nnz_t end = plan.outer_offsets[row + 1U];
    for (nnz_t position = begin; position < end; ++position) {
      const index_t column = plan.inner_indices[position];
      sum += static_cast<const Scalar*>(plan.values)[position] *
             static_cast<const Scalar*>(plan.input)[column * plan.input_stride];
    }
    Scalar* output = static_cast<Scalar*>(plan.output);
    const auto output_offset = static_cast<stride_t>(row) * plan.output_stride;
    const Scalar alpha = static_cast<Scalar>(plan.alpha);
    const Scalar beta = static_cast<Scalar>(plan.beta);
    output[output_offset] = beta == static_cast<Scalar>(0)
                                ? alpha * sum
                                : alpha * sum + beta * output[output_offset];
  }
}

Status KernelStatus(cudaError_t error, const char* operation) {
  if (error == cudaSuccess) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  return Status(ErrorCode::kProvider,
                std::string(operation) + ": " + cudaGetErrorString(error),
                "cuda", static_cast<std::int64_t>(error));
}

template <typename Plan, typename FloatKernel, typename DoubleKernel>
Status Launch(const ExecutionContext& context, const Plan& plan,
              std::uint64_t logical_size, FloatKernel float_kernel,
              DoubleKernel double_kernel, const char* operation) {
  if (logical_size == 0) {
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
  const auto required = (logical_size + kBlockSize - 1U) / kBlockSize;
  const auto blocks = static_cast<unsigned int>(
      required < kMaximumBlockCount ? required : kMaximumBlockCount);
  static_cast<void>(cudaGetLastError());
  if (plan.scalar_type == ScalarType::kFloat) {
    float_kernel<<<blocks, kBlockSize, 0,
                   reinterpret_cast<cudaStream_t>(*stream)>>>(plan);
  } else {
    double_kernel<<<blocks, kBlockSize, 0,
                    reinterpret_cast<cudaStream_t>(*stream)>>>(plan);
  }
  const cudaError_t error = cudaGetLastError();
  if (error != cudaSuccess) {
    static_cast<void>(
        cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(*stream)));
  }
  return KernelStatus(error, operation);
}

}  // namespace

Status LaunchSparsePointwiseKernel(const ExecutionContext& context,
                                   const PointwisePlan& plan) {
  return Launch(context, plan, static_cast<std::uint64_t>(plan.nnz),
                SparsePointwiseKernel<float>, SparsePointwiseKernel<double>,
                "CUDA sparse pointwise kernel launch");
}

Status LaunchStridedSpmvKernel(const ExecutionContext& context,
                               const SpmvPlan& plan) {
  return Launch(context, plan, static_cast<std::uint64_t>(plan.rows),
                StridedSpmvKernel<float>, StridedSpmvKernel<double>,
                "CUDA strided CSR SpMV kernel launch");
}

}  // namespace internal_sparse_cuda
}  // namespace asc
