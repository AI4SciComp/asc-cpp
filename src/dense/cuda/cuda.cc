#include "asc/dense/providers/cuda.h"

#include <cublas_v2.h>
#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "../../core/cuda/provider_internal.h"
#include "kernels_internal.h"

namespace asc {
namespace internal_dense_cuda {

class DenseCudaContextState {
 public:
  DenseCudaContextState(cublasHandle_t handle,
                        std::int32_t device_ordinal) noexcept
      : handle_(handle), device_ordinal_(device_ordinal) {}
  DenseCudaContextState(const DenseCudaContextState&) = delete;
  DenseCudaContextState& operator=(const DenseCudaContextState&) = delete;
  ~DenseCudaContextState() {
    if (handle_ != nullptr) {
      auto guard = internal_core_cuda::CudaDeviceGuard::Create(device_ordinal_);
      if (!guard.ok()) {
        return;
      }
      static_cast<void>(cublasDestroy(handle_));
    }
  }

  [[nodiscard]] cublasHandle_t handle() const noexcept { return handle_; }

 private:
  cublasHandle_t handle_;
  std::int32_t device_ordinal_;
};

class DenseCudaContextAccess {
 public:
  [[nodiscard]] static DenseCudaContextState* State(
      const DenseCudaContext& context) noexcept {
    return context.state_.get();
  }
};

namespace {

void ClearPriorCudaError() noexcept { static_cast<void>(cudaGetLastError()); }

Status CublasStatus(cublasStatus_t status, const char* operation) {
  if (status == CUBLAS_STATUS_SUCCESS) {
    return Status::Ok();
  }
  return Status(ErrorCode::kProvider, std::string(operation) + " failed",
                "cublas", static_cast<std::int64_t>(status));
}

Status ValidateContext(const DenseCudaContext& context) {
  if (DenseCudaContextAccess::State(context) == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from DenseCudaContext cannot execute work");
  }
  if (context.execution_context().backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "Dense CUDA requires a CUDA execution context");
  }
  return Status::Ok();
}

Result<extent_t> LogicalSize(std::uint8_t rank,
                             const std::array<extent_t, 8>& shape) {
  extent_t size = 1;
  for (std::uint8_t dimension = 0; dimension < rank; ++dimension) {
    if (shape[dimension] < 0) {
      return Status(ErrorCode::kShape, "A CUDA dense extent is negative");
    }
    if (shape[dimension] == 0) {
      return static_cast<extent_t>(0);
    }
    auto product = CheckedMultiply(size, shape[dimension]);
    if (!product.ok()) {
      return product.status();
    }
    size = *product;
  }
  return size;
}

Result<std::size_t> RequiredBytes(std::uint8_t rank,
                                  const std::array<extent_t, 8>& shape,
                                  const std::array<stride_t, 8>& strides,
                                  std::size_t element_size) {
  auto logical_size = LogicalSize(rank, shape);
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  if (*logical_size == 0) {
    return static_cast<std::size_t>(0);
  }
  extent_t maximum_offset = 0;
  for (std::uint8_t dimension = 0; dimension < rank; ++dimension) {
    if (strides[dimension] < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA dense strides cannot be negative");
    }
    auto term = CheckedMultiply(shape[dimension] - 1, strides[dimension]);
    if (!term.ok()) {
      return term.status();
    }
    auto sum = CheckedAdd(maximum_offset, *term);
    if (!sum.ok()) {
      return sum.status();
    }
    maximum_offset = *sum;
  }
  auto span = CheckedAdd(maximum_offset, static_cast<extent_t>(1));
  if (!span.ok()) {
    return span.status();
  }
  return CheckedByteCount(*span, element_size);
}

Result<std::size_t> RequiredBytes(std::uint8_t rank,
                                  const std::array<extent_t, 2>& shape,
                                  const std::array<stride_t, 2>& strides,
                                  std::size_t element_size) {
  std::array<extent_t, 8> expanded_shape{};
  std::array<stride_t, 8> expanded_strides{};
  for (std::uint8_t dimension = 0; dimension < rank; ++dimension) {
    expanded_shape[dimension] = shape[dimension];
    expanded_strides[dimension] = strides[dimension];
  }
  return RequiredBytes(rank, expanded_shape, expanded_strides, element_size);
}

Status ValidateAddressSpan(const void* pointer, std::size_t bytes) {
  if (bytes == 0) {
    return Status::Ok();
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(ErrorCode::kOverflow,
                  "A CUDA dense address span exceeds uintptr_t");
  }
  return Status::Ok();
}

bool CheckedRangesOverlap(const void* left, std::size_t left_bytes,
                          const void* right, std::size_t right_bytes) noexcept {
  if (left_bytes == 0 || right_bytes == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left);
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right);
  return left_begin < right_begin + right_bytes &&
         right_begin < left_begin + left_bytes;
}

Status ValidateDeviceSpan(const ExecutionContext& context, const void* pointer,
                          std::size_t bytes) {
  const Status address_status = ValidateAddressSpan(pointer, bytes);
  if (!address_status.ok()) {
    return address_status;
  }
  return internal_core_cuda::ValidateCudaMemory(context, pointer, bytes,
                                                MemorySpace::kDevice);
}

std::size_t ElementSize(ScalarType type) {
  return type == ScalarType::kFloat ? sizeof(float) : sizeof(double);
}

Status ValidateScalarType(ScalarType type) {
  switch (type) {
    case ScalarType::kFloat:
    case ScalarType::kDouble:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "CUDA dense scalar type is invalid");
}

Status ValidatePointwiseOperand(const ExecutionContext& context,
                                const PointwisePlan& plan,
                                const PointwiseOperand& operand,
                                std::size_t destination_bytes) {
  if (operand.is_scalar) {
    return Status::Ok();
  }
  auto bytes = RequiredBytes(plan.rank, plan.shape, operand.strides,
                             ElementSize(plan.scalar_type));
  if (!bytes.ok()) {
    return bytes.status();
  }
  const Status memory_status =
      ValidateDeviceSpan(context, operand.data, *bytes);
  if (!memory_status.ok()) {
    return memory_status;
  }
  if (CheckedRangesOverlap(operand.data, *bytes, plan.destination,
                           destination_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA evaluation rejects destination overlap");
  }
  return Status::Ok();
}

bool IsBinary(PointwiseOperation operation) {
  return operation == PointwiseOperation::kAdd ||
         operation == PointwiseOperation::kSubtract ||
         operation == PointwiseOperation::kMultiply;
}

Status ValidatePointwise(const DenseCudaContext& context, PointwisePlan& plan) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status scalar_status = ValidateScalarType(plan.scalar_type);
  if (!scalar_status.ok()) {
    return scalar_status;
  }
  if (plan.rank > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA evaluation supports rank zero through eight");
  }
  auto logical_size = LogicalSize(plan.rank, plan.shape);
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  plan.logical_size = *logical_size;
  auto destination_bytes =
      RequiredBytes(plan.rank, plan.shape, plan.destination_strides,
                    ElementSize(plan.scalar_type));
  if (!destination_bytes.ok()) {
    return destination_bytes.status();
  }
  const Status destination_status = ValidateDeviceSpan(
      context.execution_context(), plan.destination, *destination_bytes);
  if (!destination_status.ok()) {
    return destination_status;
  }
  if (plan.no_op) {
    return Status::Ok();
  }
  const Status left_status = ValidatePointwiseOperand(
      context.execution_context(), plan, plan.left, *destination_bytes);
  if (!left_status.ok()) {
    return left_status;
  }
  if (IsBinary(plan.operation)) {
    return ValidatePointwiseOperand(context.execution_context(), plan,
                                    plan.right, *destination_bytes);
  }
  return Status::Ok();
}

Status ValidateOperation(MatrixOperation operation) {
  if (operation != MatrixOperation::kNone &&
      operation != MatrixOperation::kTranspose) {
    return Status(ErrorCode::kInvalidArgument,
                  "MatrixOperation is not a recognized enumerator");
  }
  return Status::Ok();
}

bool IsColumnMajorCompatible(const LinalgOperand& matrix) {
  if (matrix.shape[0] == 0 || matrix.shape[1] == 0) {
    return true;
  }
  return matrix.strides[0] == 1 &&
         matrix.strides[1] >= std::max<stride_t>(1, matrix.shape[0]);
}

bool IsColumnMajorCompatible(const LinalgPlan& plan) {
  if (plan.destination_shape[0] == 0 || plan.destination_shape[1] == 0) {
    return true;
  }
  return plan.destination_strides[0] == 1 &&
         plan.destination_strides[1] >=
             std::max<stride_t>(1, plan.destination_shape[0]);
}

Status ValidateLinalgSpan(const ExecutionContext& context,
                          const LinalgOperand& operand, std::uint8_t rank,
                          std::size_t element_size, std::size_t& bytes) {
  auto required =
      RequiredBytes(rank, operand.shape, operand.strides, element_size);
  if (!required.ok()) {
    return required.status();
  }
  bytes = *required;
  return ValidateDeviceSpan(context, operand.data, bytes);
}

Status ValidateLinalg(const DenseCudaContext& context, LinalgPlan& plan) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status scalar_status = ValidateScalarType(plan.scalar_type);
  if (!scalar_status.ok()) {
    return scalar_status;
  }
  if (plan.rank > 2) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA dense algebra supports rank one or two");
  }
  const std::size_t element_size = ElementSize(plan.scalar_type);
  auto destination_bytes =
      RequiredBytes(plan.rank, plan.destination_shape, plan.destination_strides,
                    element_size);
  if (!destination_bytes.ok()) {
    return destination_bytes.status();
  }
  const Status destination_address_status =
      ValidateAddressSpan(plan.destination, *destination_bytes);
  if (!destination_address_status.ok()) {
    return destination_address_status;
  }
  std::array<extent_t, 8> destination_shape{};
  for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
    destination_shape[dimension] = plan.destination_shape[dimension];
  }
  auto logical_size = LogicalSize(plan.rank, destination_shape);
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  plan.logical_size = *logical_size;

  if (plan.no_op && plan.operation == LinalgOperation::kCopy) {
    const std::size_t inspected_bytes =
        plan.logical_size == 0 ? 0 : element_size;
    return internal_core_cuda::ValidateCudaMemory(
        context.execution_context(), plan.destination, inspected_bytes,
        MemorySpace::kDevice);
  }

  if (plan.operation == LinalgOperation::kScal ||
      plan.operation == LinalgOperation::kScaleOrZero) {
    return internal_core_cuda::ValidateCudaMemory(
        context.execution_context(), plan.destination, *destination_bytes,
        MemorySpace::kDevice);
  }
  if (plan.operation == LinalgOperation::kCopy ||
      plan.operation == LinalgOperation::kAxpy) {
    if (plan.left.shape != plan.destination_shape) {
      return Status(ErrorCode::kShape,
                    "CUDA dense algebra operand shapes differ");
    }
    const Status destination_status = internal_core_cuda::ValidateCudaMemory(
        context.execution_context(), plan.destination, *destination_bytes,
        MemorySpace::kDevice);
    if (!destination_status.ok()) {
      return destination_status;
    }
    std::size_t source_bytes = 0;
    const Status source_status =
        ValidateLinalgSpan(context.execution_context(), plan.left, plan.rank,
                           element_size, source_bytes);
    if (!source_status.ok()) {
      return source_status;
    }
    const bool exact_in_place_axpy =
        plan.operation == LinalgOperation::kAxpy &&
        plan.left.data == plan.destination &&
        plan.left.shape == plan.destination_shape &&
        plan.left.strides == plan.destination_strides;
    if (CheckedRangesOverlap(plan.left.data, source_bytes, plan.destination,
                             *destination_bytes) &&
        !exact_in_place_axpy) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA dense algebra rejects output overlap");
    }
    return Status::Ok();
  }

  const Status left_operation_status = ValidateOperation(plan.left_operation);
  if (!left_operation_status.ok()) {
    return left_operation_status;
  }
  if (plan.operation == LinalgOperation::kGemm) {
    const Status right_operation_status =
        ValidateOperation(plan.right_operation);
    if (!right_operation_status.ok()) {
      return right_operation_status;
    }
  }
  if (!IsColumnMajorCompatible(plan.left) ||
      (plan.operation == LinalgOperation::kGemm &&
       !IsColumnMajorCompatible(plan.right)) ||
      (plan.operation == LinalgOperation::kGemm &&
       !IsColumnMajorCompatible(plan))) {
    return Status(ErrorCode::kUnsupported,
                  "cuBLAS requires column-major compatible matrix mappings");
  }

  const extent_t row_count = plan.left_operation == MatrixOperation::kNone
                                 ? plan.left.shape[0]
                                 : plan.left.shape[1];
  const extent_t inner_count = plan.left_operation == MatrixOperation::kNone
                                   ? plan.left.shape[1]
                                   : plan.left.shape[0];
  extent_t column_count = 1;
  extent_t right_inner_count = plan.right.shape[0];
  if (plan.operation == LinalgOperation::kGemm) {
    right_inner_count = plan.right_operation == MatrixOperation::kNone
                            ? plan.right.shape[0]
                            : plan.right.shape[1];
    column_count = plan.right_operation == MatrixOperation::kNone
                       ? plan.right.shape[1]
                       : plan.right.shape[0];
  }
  auto checked_rows = CheckedCast<int>(row_count);
  auto checked_inner = CheckedCast<int>(inner_count);
  auto checked_columns = CheckedCast<int>(column_count);
  if (!checked_rows.ok() || !checked_inner.ok() || !checked_columns.ok()) {
    return Status(ErrorCode::kOverflow,
                  "A cuBLAS dimension exceeds provider integer width");
  }
  const stride_t left_leading_dimension =
      std::max<stride_t>(1, plan.left.strides[1]);
  auto checked_left_leading_dimension =
      CheckedCast<int>(left_leading_dimension);
  if (!checked_left_leading_dimension.ok()) {
    return Status(ErrorCode::kOverflow,
                  "A cuBLAS leading dimension exceeds provider integer width");
  }
  if (plan.operation == LinalgOperation::kGemv) {
    const extent_t input_length = plan.right.shape[0];
    const extent_t output_length = plan.destination_shape[0];
    const stride_t input_increment =
        input_length <= 1 ? 1 : plan.right.strides[0];
    const stride_t output_increment =
        output_length <= 1 ? 1 : plan.destination_strides[0];
    if (input_increment <= 0 || output_increment <= 0 ||
        !CheckedCast<int>(input_increment).ok() ||
        !CheckedCast<int>(output_increment).ok()) {
      return Status(ErrorCode::kOverflow,
                    "A cuBLAS vector increment exceeds provider width");
    }
  } else {
    const stride_t right_leading_dimension =
        std::max<stride_t>(1, plan.right.strides[1]);
    const stride_t output_leading_dimension =
        std::max<stride_t>(1, plan.destination_strides[1]);
    if (!CheckedCast<int>(right_leading_dimension).ok() ||
        !CheckedCast<int>(output_leading_dimension).ok()) {
      return Status(
          ErrorCode::kOverflow,
          "A cuBLAS leading dimension exceeds provider integer width");
    }
  }
  if (plan.operation == LinalgOperation::kGemv) {
    if (plan.right.shape[0] != inner_count ||
        plan.destination_shape[0] != row_count) {
      return Status(ErrorCode::kShape,
                    "CudaGemv operand shapes are incompatible");
    }
  } else if (inner_count != right_inner_count ||
             plan.destination_shape[0] != row_count ||
             plan.destination_shape[1] != column_count) {
    return Status(ErrorCode::kShape,
                  "CudaGemm operand shapes are incompatible");
  }

  const Status destination_status = internal_core_cuda::ValidateCudaMemory(
      context.execution_context(), plan.destination, *destination_bytes,
      MemorySpace::kDevice);
  if (!destination_status.ok()) {
    return destination_status;
  }
  std::size_t left_bytes = 0;
  std::size_t right_bytes = 0;
  Status status = ValidateLinalgSpan(context.execution_context(), plan.left, 2,
                                     element_size, left_bytes);
  if (!status.ok()) {
    return status;
  }
  status = ValidateLinalgSpan(context.execution_context(), plan.right,
                              plan.operation == LinalgOperation::kGemv ? 1 : 2,
                              element_size, right_bytes);
  if (!status.ok()) {
    return status;
  }
  if (CheckedRangesOverlap(plan.left.data, left_bytes, plan.destination,
                           *destination_bytes) ||
      CheckedRangesOverlap(plan.right.data, right_bytes, plan.destination,
                           *destination_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA dense algebra rejects output overlap");
  }
  return Status::Ok();
}

template <typename Scalar>
Result<CompletionEvent> LaunchCublas(const DenseCudaContext& context,
                                     const LinalgPlan& plan) {
  const ExecutionContext& execution = context.execution_context();
  auto pending = internal_core_cuda::PendingCudaEvent::Create(execution);
  if (!pending.ok()) {
    return pending.status();
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(execution);
  if (!guard.ok()) {
    return guard.status();
  }
  cublasHandle_t handle = DenseCudaContextAccess::State(context)->handle();
  const Scalar alpha = static_cast<Scalar>(plan.alpha);
  const Scalar beta = static_cast<Scalar>(plan.beta);
  cublasStatus_t status = CUBLAS_STATUS_SUCCESS;
  if (plan.operation == LinalgOperation::kGemv) {
    const int m = static_cast<int>(plan.left.shape[0]);
    const int n = static_cast<int>(plan.left.shape[1]);
    const int lda =
        static_cast<int>(std::max<stride_t>(1, plan.left.strides[1]));
    const int input_length =
        plan.left_operation == MatrixOperation::kNone ? n : m;
    const int output_length =
        plan.left_operation == MatrixOperation::kNone ? m : n;
    const int incx =
        input_length <= 1 ? 1 : static_cast<int>(plan.right.strides[0]);
    const int incy =
        output_length <= 1 ? 1 : static_cast<int>(plan.destination_strides[0]);
    const cublasOperation_t operation =
        plan.left_operation == MatrixOperation::kNone ? CUBLAS_OP_N
                                                      : CUBLAS_OP_T;
    if constexpr (std::same_as<Scalar, float>) {
      status = cublasSgemv(handle, operation, m, n, &alpha,
                           static_cast<const float*>(plan.left.data), lda,
                           static_cast<const float*>(plan.right.data), incx,
                           &beta, static_cast<float*>(plan.destination), incy);
    } else {
      status = cublasDgemv(handle, operation, m, n, &alpha,
                           static_cast<const double*>(plan.left.data), lda,
                           static_cast<const double*>(plan.right.data), incx,
                           &beta, static_cast<double*>(plan.destination), incy);
    }
  } else {
    const int m = static_cast<int>(plan.destination_shape[0]);
    const int n = static_cast<int>(plan.destination_shape[1]);
    const int k = static_cast<int>(plan.left_operation == MatrixOperation::kNone
                                       ? plan.left.shape[1]
                                       : plan.left.shape[0]);
    const int lda =
        static_cast<int>(std::max<stride_t>(1, plan.left.strides[1]));
    const int ldb =
        static_cast<int>(std::max<stride_t>(1, plan.right.strides[1]));
    const int ldc =
        static_cast<int>(std::max<stride_t>(1, plan.destination_strides[1]));
    const cublasOperation_t left_operation =
        plan.left_operation == MatrixOperation::kNone ? CUBLAS_OP_N
                                                      : CUBLAS_OP_T;
    const cublasOperation_t right_operation =
        plan.right_operation == MatrixOperation::kNone ? CUBLAS_OP_N
                                                       : CUBLAS_OP_T;
    if constexpr (std::same_as<Scalar, float>) {
      status = cublasSgemm(handle, left_operation, right_operation, m, n, k,
                           &alpha, static_cast<const float*>(plan.left.data),
                           lda, static_cast<const float*>(plan.right.data), ldb,
                           &beta, static_cast<float*>(plan.destination), ldc);
    } else {
      status =
          cublasDgemm(handle, left_operation, right_operation, m, n, k, &alpha,
                      static_cast<const double*>(plan.left.data), lda,
                      static_cast<const double*>(plan.right.data), ldb, &beta,
                      static_cast<double*>(plan.destination), ldc);
    }
  }
  if (status != CUBLAS_STATUS_SUCCESS) {
    // A provider failure may be reported after work was accepted. Drain only
    // this context's stream before returning without a completion event, while
    // preserving the original cuBLAS diagnostic. The drain precedes diagnostic
    // construction so an allocation failure cannot bypass the safety action.
    auto stream = internal_core_cuda::CudaStreamHandle(execution);
    if (stream.ok()) {
      static_cast<void>(
          cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(*stream)));
      // The failure-only drain is handled here and must not poison the outer
      // device guard's restoration. Preserve the original cuBLAS status below.
      static_cast<void>(cudaGetLastError());
    }
    return CublasStatus(status, "cuBLAS operation");
  }
  return pending->Record();
}

Result<CompletionEvent> LaunchKernelLinalg(const DenseCudaContext& context,
                                           const LinalgPlan& plan) {
  auto pending =
      internal_core_cuda::PendingCudaEvent::Create(context.execution_context());
  if (!pending.ok()) {
    return pending.status();
  }
  const Status launch_status =
      LaunchBasicLinalgKernel(context.execution_context(), plan);
  if (!launch_status.ok()) {
    return launch_status;
  }
  return pending->Record();
}

}  // namespace

Result<CompletionEvent> LaunchPointwiseOpaque(const DenseCudaContext& context,
                                              const void* opaque_plan) {
  ClearPriorCudaError();
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA pointwise plan cannot be null");
  }
  PointwisePlan plan = *static_cast<const PointwisePlan*>(opaque_plan);
  const Status validation = ValidatePointwise(context, plan);
  if (!validation.ok()) {
    return validation;
  }
  auto pending =
      internal_core_cuda::PendingCudaEvent::Create(context.execution_context());
  if (!pending.ok()) {
    return pending.status();
  }
  const Status launch_status =
      LaunchPointwiseKernel(context.execution_context(), plan);
  if (!launch_status.ok()) {
    return launch_status;
  }
  return pending->Record();
}

Result<CompletionEvent> LaunchLinalgOpaque(const DenseCudaContext& context,
                                           const void* opaque_plan) {
  ClearPriorCudaError();
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA linalg plan cannot be null");
  }
  LinalgPlan plan = *static_cast<const LinalgPlan*>(opaque_plan);
  const Status validation = ValidateLinalg(context, plan);
  if (!validation.ok()) {
    return validation;
  }
  if (plan.operation == LinalgOperation::kGemv ||
      plan.operation == LinalgOperation::kGemm) {
    const extent_t inner_count = plan.left_operation == MatrixOperation::kNone
                                     ? plan.left.shape[1]
                                     : plan.left.shape[0];
    if (plan.logical_size == 0) {
      auto pending = internal_core_cuda::PendingCudaEvent::Create(
          context.execution_context());
      if (!pending.ok()) {
        return pending.status();
      }
      return pending->Record();
    }
    if (inner_count == 0) {
      plan.operation = LinalgOperation::kScaleOrZero;
      plan.alpha = plan.beta;
      return LaunchKernelLinalg(context, plan);
    }
    if (plan.scalar_type == ScalarType::kFloat) {
      return LaunchCublas<float>(context, plan);
    }
    return LaunchCublas<double>(context, plan);
  }
  return LaunchKernelLinalg(context, plan);
}

}  // namespace internal_dense_cuda

DenseCudaContext::DenseCudaContext(
    ExecutionContext execution_context,
    std::unique_ptr<internal_dense_cuda::DenseCudaContextState> state) noexcept
    : execution_context_(std::move(execution_context)),
      state_(std::move(state)) {}

Result<DenseCudaContext> DenseCudaContext::Create(
    ExecutionContext execution_context) {
  internal_dense_cuda::ClearPriorCudaError();
  if (execution_context.backend() != Backend::kCuda ||
      execution_context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "DenseCudaContext requires a CUDA execution context");
  }
  auto stream = internal_core_cuda::CudaStreamHandle(execution_context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(execution_context);
  if (!guard.ok()) {
    return guard.status();
  }
  cublasHandle_t handle = nullptr;
  cublasStatus_t cublas_status = cublasCreate(&handle);
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return internal_dense_cuda::CublasStatus(cublas_status, "cublasCreate");
  }
  auto state = std::make_unique<internal_dense_cuda::DenseCudaContextState>(
      handle, execution_context.device().ordinal);
  cublas_status =
      cublasSetStream(handle, reinterpret_cast<cudaStream_t>(*stream));
  if (cublas_status == CUBLAS_STATUS_SUCCESS) {
    cublas_status = cublasSetPointerMode(handle, CUBLAS_POINTER_MODE_HOST);
  }
  if (cublas_status == CUBLAS_STATUS_SUCCESS) {
    cublas_status = cublasSetAtomicsMode(handle, CUBLAS_ATOMICS_NOT_ALLOWED);
  }
  if (cublas_status == CUBLAS_STATUS_SUCCESS) {
    const cublasMath_t math_mode =
        execution_context.determinism() == Determinism::kDeterministic
            ? CUBLAS_PEDANTIC_MATH
            : CUBLAS_DEFAULT_MATH;
    cublas_status = cublasSetMathMode(handle, math_mode);
  }
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return internal_dense_cuda::CublasStatus(
        cublas_status, "DenseCudaContext provider configuration");
  }
  return DenseCudaContext(std::move(execution_context), std::move(state));
}

DenseCudaContext::DenseCudaContext(DenseCudaContext&& other) noexcept
    : execution_context_(other.execution_context_),
      state_(std::move(other.state_)) {}

DenseCudaContext& DenseCudaContext::operator=(
    DenseCudaContext&& other) noexcept {
  if (this != &other) {
    state_ = std::move(other.state_);
    execution_context_ = other.execution_context_;
  }
  return *this;
}

DenseCudaContext::~DenseCudaContext() = default;

const ExecutionContext& DenseCudaContext::execution_context() const noexcept {
  return execution_context_;
}

}  // namespace asc
