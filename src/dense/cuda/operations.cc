#include <cublas_v2.h>
#include <cuda_runtime_api.h>

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"
#include "context_internal.h"
#include "kernels_internal.h"
#include "validation_internal.h"

namespace asc::internal_dense_cuda {
namespace {

Result<void*> ValidateContext(DenseCudaContext& context) {
  ContextState* dense_state = Access::State(context);
  if (dense_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from DenseCudaContext cannot launch work");
  }
  const ExecutionContext& execution = context.execution_context();
  if (execution.backend() != Backend::kCuda ||
      execution.device().ordinal != dense_state->device()) {
    return Status(ErrorCode::kInvalidState,
                  "Dense CUDA provider state does not match its context");
  }
  const auto& execution_state =
      internal_core_execution::Access::State(execution);
  if (execution_state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The Dense CUDA execution context has no provider state");
  }
  void* stream = execution_state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The Dense CUDA execution context has no stream");
  }
  return stream;
}

void DrainStream(void* stream) noexcept {
  static_cast<void>(cudaStreamSynchronize(static_cast<cudaStream_t>(stream)));
}

Status ValidateOperand(const OperandDescriptor& operand,
                       const ViewDescriptor& destination, std::int32_t device) {
  if (operand.kind == OperandKind::kScalar) {
    if (operand.view.element_kind != destination.element_kind) {
      return Status(ErrorCode::kInvalidArgument,
                    "A CUDA scalar type does not match its destination");
    }
    return Status::Ok();
  }
  if (operand.kind != OperandKind::kView) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA pointwise operand kind is not recognized");
  }
  Status metadata =
      ValidateViewMetadata(operand.view, destination.element_kind, 8);
  if (!metadata.ok()) {
    return metadata;
  }
  Status shape = ValidateSameShape(operand.view, destination);
  if (!shape.ok()) {
    return shape;
  }
  return ValidateDevicePointer(operand.view, device);
}

Result<CompletionEvent> Finish(DenseCudaContext& context, void* stream,
                               Status launch_status) {
  if (!launch_status.ok()) {
    DrainStream(stream);
    return launch_status;
  }
  return RecordCudaEvent(context.execution_context());
}

template <typename Element, std::size_t Rank>
Result<CompletionEvent> CopyImpl(DenseCudaContext& context,
                                 DenseView<const Element, Rank> source,
                                 DenseView<Element, Rank> destination) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const ViewDescriptor source_descriptor = Describe(source);
  const ViewDescriptor destination_descriptor = Describe(destination);
  for (const ViewDescriptor* descriptor :
       {&source_descriptor, &destination_descriptor}) {
    Status metadata =
        ValidateViewMetadata(*descriptor, source_descriptor.element_kind, Rank);
    if (!metadata.ok()) {
      return metadata;
    }
    Status pointer = ValidateDevicePointer(
        *descriptor, context.execution_context().device().ordinal);
    if (!pointer.ok()) {
      return pointer;
    }
  }
  Status shape = ValidateSameShape(source_descriptor, destination_descriptor);
  if (!shape.ok()) {
    return shape;
  }
  if (SameDescriptor(source_descriptor, destination_descriptor)) {
    auto bytes = ViewBytes(source_descriptor);
    if (!bytes.ok()) {
      return bytes.status();
    }
    return CopyBytes(
        context.execution_context(),
        MutableMemoryView(destination.data(), *bytes, MemorySpace::kDevice),
        ConstMemoryView(source.data(), *bytes, MemorySpace::kDevice));
  }
  auto overlap = ViewsOverlap(source_descriptor, destination_descriptor);
  if (!overlap.ok()) {
    return overlap.status();
  }
  if (*overlap) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaCopy rejects partially overlapping DenseViews");
  }
  return Finish(
      context, *stream,
      LaunchCopyKernel(*stream, source_descriptor, destination_descriptor));
}

template <typename Element, std::size_t Rank>
Result<CompletionEvent> ScalImpl(DenseCudaContext& context, Element alpha,
                                 DenseView<Element, Rank> destination) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const ViewDescriptor descriptor = Describe(destination);
  Status metadata =
      ValidateViewMetadata(descriptor, descriptor.element_kind, Rank);
  if (!metadata.ok()) {
    return metadata;
  }
  Status pointer = ValidateDevicePointer(
      descriptor, context.execution_context().device().ordinal);
  if (!pointer.ok()) {
    return pointer;
  }
  return Finish(
      context, *stream,
      LaunchScalKernel(*stream, static_cast<double>(alpha), descriptor));
}

template <typename Element, std::size_t Rank>
Result<CompletionEvent> AxpyImpl(DenseCudaContext& context, Element alpha,
                                 DenseView<const Element, Rank> source,
                                 DenseView<Element, Rank> destination) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  const ViewDescriptor source_descriptor = Describe(source);
  const ViewDescriptor destination_descriptor = Describe(destination);
  for (const ViewDescriptor* descriptor :
       {&source_descriptor, &destination_descriptor}) {
    Status metadata =
        ValidateViewMetadata(*descriptor, source_descriptor.element_kind, Rank);
    if (!metadata.ok()) {
      return metadata;
    }
    Status pointer = ValidateDevicePointer(
        *descriptor, context.execution_context().device().ordinal);
    if (!pointer.ok()) {
      return pointer;
    }
  }
  Status shape = ValidateSameShape(source_descriptor, destination_descriptor);
  if (!shape.ok()) {
    return shape;
  }
  auto overlap = ViewsOverlap(source_descriptor, destination_descriptor);
  if (!overlap.ok()) {
    return overlap.status();
  }
  if (*overlap && !SameDescriptor(source_descriptor, destination_descriptor)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CudaAxpy rejects partial operand overlap");
  }
  return Finish(context, *stream,
                LaunchAxpyKernel(*stream, static_cast<double>(alpha),
                                 source_descriptor, destination_descriptor));
}

template <typename Element>
Result<CompletionEvent> GemvImpl(DenseCudaContext& context,
                                 MatrixOperation operation, Element alpha,
                                 DenseView<const Element, 2> matrix,
                                 DenseView<const Element, 1> input,
                                 Element beta, DenseView<Element, 1> output) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  Status operation_status = ValidateMatrixOperation(operation);
  if (!operation_status.ok()) {
    return operation_status;
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }

  const ViewDescriptor matrix_descriptor = Describe(matrix);
  const ViewDescriptor input_descriptor = Describe(input);
  const ViewDescriptor output_descriptor = Describe(output);
  for (const ViewDescriptor* descriptor :
       {&matrix_descriptor, &input_descriptor, &output_descriptor}) {
    Status metadata =
        ValidateViewMetadata(*descriptor, matrix_descriptor.element_kind, 2);
    if (!metadata.ok()) {
      return metadata;
    }
    Status pointer = ValidateDevicePointer(
        *descriptor, context.execution_context().device().ordinal);
    if (!pointer.ok()) {
      return pointer;
    }
  }
  Status mapping = ValidateMatrixMapping(matrix_descriptor);
  if (!mapping.ok()) {
    return mapping;
  }

  const extent_t output_size = operation == MatrixOperation::kNone
                                   ? matrix.extents()[0]
                                   : matrix.extents()[1];
  const extent_t inner_size = operation == MatrixOperation::kNone
                                  ? matrix.extents()[1]
                                  : matrix.extents()[0];
  if (input.extents()[0] != inner_size || output.extents()[0] != output_size) {
    return Status(ErrorCode::kShape, "CudaGemv operand shapes do not match");
  }
  for (const ViewDescriptor* input_view :
       {&matrix_descriptor, &input_descriptor}) {
    auto overlap = ViewsOverlap(*input_view, output_descriptor);
    if (!overlap.ok()) {
      return overlap.status();
    }
    if (*overlap) {
      return Status(ErrorCode::kInvalidArgument,
                    "CudaGemv rejects output overlap with an input");
    }
  }

  auto m = ProviderExtentInteger(matrix.extents()[0],
                                 "CudaGemv row count exceeds provider range");
  auto n = ProviderExtentInteger(
      matrix.extents()[1], "CudaGemv column count exceeds provider range");
  auto lda = ProviderStrideInteger(
      std::max<stride_t>(1, matrix.strides()[1]),
      "CudaGemv leading dimension exceeds provider range");
  auto incx =
      ProviderStrideInteger(std::max<stride_t>(1, input.strides()[0]),
                            "CudaGemv input increment exceeds provider range");
  auto incy =
      ProviderStrideInteger(std::max<stride_t>(1, output.strides()[0]),
                            "CudaGemv output increment exceeds provider range");
  for (const Status* status : {&m.status(), &n.status(), &lda.status(),
                               &incx.status(), &incy.status()}) {
    if (!status->ok()) {
      return *status;
    }
  }

  if (output_size == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  if (inner_size == 0) {
    const Status launch =
        beta == Element{0}
            ? LaunchFillKernel(*stream, 0.0, output_descriptor)
            : LaunchScalKernel(*stream, static_cast<double>(beta),
                               output_descriptor);
    return Finish(context, *stream, launch);
  }

  ContextState* state = Access::State(context);
  auto handle_lock = state->Lock();
  const cublasOperation_t provider_operation =
      operation == MatrixOperation::kNone ? CUBLAS_OP_N : CUBLAS_OP_T;
  cublasStatus_t provider_status;
  if constexpr (std::same_as<Element, float>) {
    provider_status = cublasSgemv(state->handle(), provider_operation, *m, *n,
                                  &alpha, matrix.data(), *lda, input.data(),
                                  *incx, &beta, output.data(), *incy);
  } else {
    provider_status = cublasDgemv(state->handle(), provider_operation, *m, *n,
                                  &alpha, matrix.data(), *lda, input.data(),
                                  *incx, &beta, output.data(), *incy);
  }
  if (provider_status != CUBLAS_STATUS_SUCCESS) {
    const Status failure =
        CublasStatus(provider_status, "cuBLAS could not enqueue CudaGemv");
    DrainStream(*stream);
    return failure;
  }
  return RecordCudaEvent(context.execution_context());
}

template <typename Element>
Result<CompletionEvent> GemmImpl(DenseCudaContext& context,
                                 MatrixOperation left_operation,
                                 MatrixOperation right_operation, Element alpha,
                                 DenseView<const Element, 2> left,
                                 DenseView<const Element, 2> right,
                                 Element beta, DenseView<Element, 2> output) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  for (MatrixOperation operation : {left_operation, right_operation}) {
    Status status = ValidateMatrixOperation(operation);
    if (!status.ok()) {
      return status;
    }
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }

  const ViewDescriptor left_descriptor = Describe(left);
  const ViewDescriptor right_descriptor = Describe(right);
  const ViewDescriptor output_descriptor = Describe(output);
  for (const ViewDescriptor* descriptor :
       {&left_descriptor, &right_descriptor, &output_descriptor}) {
    Status metadata =
        ValidateViewMetadata(*descriptor, left_descriptor.element_kind, 2);
    if (!metadata.ok()) {
      return metadata;
    }
    Status pointer = ValidateDevicePointer(
        *descriptor, context.execution_context().device().ordinal);
    if (!pointer.ok()) {
      return pointer;
    }
    Status mapping = ValidateMatrixMapping(*descriptor);
    if (!mapping.ok()) {
      return mapping;
    }
  }

  const extent_t m = left_operation == MatrixOperation::kNone
                         ? left.extents()[0]
                         : left.extents()[1];
  const extent_t left_inner = left_operation == MatrixOperation::kNone
                                  ? left.extents()[1]
                                  : left.extents()[0];
  const extent_t right_inner = right_operation == MatrixOperation::kNone
                                   ? right.extents()[0]
                                   : right.extents()[1];
  const extent_t n = right_operation == MatrixOperation::kNone
                         ? right.extents()[1]
                         : right.extents()[0];
  if (left_inner != right_inner || output.extents()[0] != m ||
      output.extents()[1] != n) {
    return Status(ErrorCode::kShape, "CudaGemm operand shapes do not match");
  }
  for (const ViewDescriptor* input_view :
       {&left_descriptor, &right_descriptor}) {
    auto overlap = ViewsOverlap(*input_view, output_descriptor);
    if (!overlap.ok()) {
      return overlap.status();
    }
    if (*overlap) {
      return Status(ErrorCode::kInvalidArgument,
                    "CudaGemm rejects output overlap with an input");
    }
  }

  auto provider_m =
      ProviderExtentInteger(m, "CudaGemm row count exceeds provider range");
  auto provider_n =
      ProviderExtentInteger(n, "CudaGemm column count exceeds provider range");
  auto provider_k = ProviderExtentInteger(
      left_inner, "CudaGemm inner dimension exceeds provider range");
  auto lda = ProviderStrideInteger(
      std::max<stride_t>(1, left.strides()[1]),
      "CudaGemm left leading dimension exceeds provider range");
  auto ldb = ProviderStrideInteger(
      std::max<stride_t>(1, right.strides()[1]),
      "CudaGemm right leading dimension exceeds provider range");
  auto ldc = ProviderStrideInteger(
      std::max<stride_t>(1, output.strides()[1]),
      "CudaGemm output leading dimension exceeds provider range");
  for (const Status* status :
       {&provider_m.status(), &provider_n.status(), &provider_k.status(),
        &lda.status(), &ldb.status(), &ldc.status()}) {
    if (!status->ok()) {
      return *status;
    }
  }

  if (m == 0 || n == 0) {
    return RecordCudaEvent(context.execution_context());
  }
  if (left_inner == 0) {
    const Status launch =
        beta == Element{0}
            ? LaunchFillKernel(*stream, 0.0, output_descriptor)
            : LaunchScalKernel(*stream, static_cast<double>(beta),
                               output_descriptor);
    return Finish(context, *stream, launch);
  }

  ContextState* state = Access::State(context);
  auto handle_lock = state->Lock();
  const cublasOperation_t provider_left =
      left_operation == MatrixOperation::kNone ? CUBLAS_OP_N : CUBLAS_OP_T;
  const cublasOperation_t provider_right =
      right_operation == MatrixOperation::kNone ? CUBLAS_OP_N : CUBLAS_OP_T;
  cublasStatus_t provider_status;
  if constexpr (std::same_as<Element, float>) {
    provider_status =
        cublasSgemm(state->handle(), provider_left, provider_right, *provider_m,
                    *provider_n, *provider_k, &alpha, left.data(), *lda,
                    right.data(), *ldb, &beta, output.data(), *ldc);
  } else {
    provider_status =
        cublasDgemm(state->handle(), provider_left, provider_right, *provider_m,
                    *provider_n, *provider_k, &alpha, left.data(), *lda,
                    right.data(), *ldb, &beta, output.data(), *ldc);
  }
  if (provider_status != CUBLAS_STATUS_SUCCESS) {
    const Status failure =
        CublasStatus(provider_status, "cuBLAS could not enqueue CudaGemm");
    DrainStream(*stream);
    return failure;
  }
  return RecordCudaEvent(context.execution_context());
}

}  // namespace

Result<CompletionEvent> CudaEvaluateErased(DenseCudaContext& context,
                                           PointwiseOperation operation,
                                           OperandDescriptor left,
                                           OperandDescriptor right,
                                           ViewDescriptor destination) {
  auto stream = ValidateContext(context);
  if (!stream.ok()) {
    return stream.status();
  }
  auto guard = internal_core_cuda::DeviceGuard::Create(
      context.execution_context().device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status destination_metadata =
      ValidateViewMetadata(destination, destination.element_kind, 8);
  if (!destination_metadata.ok()) {
    return destination_metadata;
  }
  Status destination_pointer = ValidateDevicePointer(
      destination, context.execution_context().device().ordinal);
  if (!destination_pointer.ok()) {
    return destination_pointer;
  }

  const bool binary = operation == PointwiseOperation::kAdd ||
                      operation == PointwiseOperation::kSubtract ||
                      operation == PointwiseOperation::kMultiply;
  switch (operation) {
    case PointwiseOperation::kCopy:
    case PointwiseOperation::kFill:
    case PointwiseOperation::kNegate:
    case PointwiseOperation::kAdd:
    case PointwiseOperation::kSubtract:
    case PointwiseOperation::kMultiply:
      break;
    default:
      return Status(ErrorCode::kInvalidArgument,
                    "A CUDA pointwise operation is not recognized");
  }
  Status left_status = ValidateOperand(
      left, destination, context.execution_context().device().ordinal);
  if (!left_status.ok()) {
    return left_status;
  }
  if (binary) {
    Status right_status = ValidateOperand(
        right, destination, context.execution_context().device().ordinal);
    if (!right_status.ok()) {
      return right_status;
    }
  }
  if ((operation == PointwiseOperation::kCopy ||
       operation == PointwiseOperation::kNegate) &&
      left.kind != OperandKind::kView) {
    return Status(ErrorCode::kUnsupported,
                  "This CUDA pointwise operation requires a Dense terminal");
  }
  if (operation == PointwiseOperation::kFill &&
      left.kind != OperandKind::kScalar) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA fill requires a rank-zero scalar");
  }

  if (operation == PointwiseOperation::kCopy &&
      SameDescriptor(left.view, destination)) {
    auto bytes = ViewBytes(destination);
    if (!bytes.ok()) {
      return bytes.status();
    }
    return CopyBytes(
        context.execution_context(),
        MutableMemoryView(const_cast<void*>(destination.data), *bytes,
                          MemorySpace::kDevice),
        ConstMemoryView(left.view.data, *bytes, MemorySpace::kDevice));
  }
  const auto validate_overlap =
      [&](const OperandDescriptor& operand) -> Status {
    if (operand.kind != OperandKind::kView) {
      return Status::Ok();
    }
    auto overlap = ViewsOverlap(operand.view, destination);
    if (!overlap.ok()) {
      return overlap.status();
    }
    if (*overlap) {
      return Status(
          ErrorCode::kInvalidArgument,
          "CUDA evaluation rejects destination overlap with an operand");
    }
    return Status::Ok();
  };
  Status left_overlap = validate_overlap(left);
  if (!left_overlap.ok()) {
    return left_overlap;
  }
  if (binary) {
    Status right_overlap = validate_overlap(right);
    if (!right_overlap.ok()) {
      return right_overlap;
    }
  }
  return Finish(
      context, *stream,
      LaunchPointwiseKernel(*stream, operation, left, right, destination));
}

}  // namespace asc::internal_dense_cuda

namespace asc {

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 DenseView<const float, 1> source,
                                 DenseView<float, 1> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 DenseView<const float, 2> source,
                                 DenseView<float, 2> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 DenseView<const double, 1> source,
                                 DenseView<double, 1> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaCopy(DenseCudaContext& context,
                                 DenseView<const double, 2> source,
                                 DenseView<double, 2> destination) {
  return internal_dense_cuda::CopyImpl(context, source, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, float alpha,
                                 DenseView<float, 1> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, float alpha,
                                 DenseView<float, 2> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, double alpha,
                                 DenseView<double, 1> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaScal(DenseCudaContext& context, double alpha,
                                 DenseView<double, 2> destination) {
  return internal_dense_cuda::ScalImpl(context, alpha, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context, float alpha,
                                 DenseView<const float, 1> source,
                                 DenseView<float, 1> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context, float alpha,
                                 DenseView<const float, 2> source,
                                 DenseView<float, 2> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context, double alpha,
                                 DenseView<const double, 1> source,
                                 DenseView<double, 1> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaAxpy(DenseCudaContext& context, double alpha,
                                 DenseView<const double, 2> source,
                                 DenseView<double, 2> destination) {
  return internal_dense_cuda::AxpyImpl(context, alpha, source, destination);
}

Result<CompletionEvent> CudaGemv(DenseCudaContext& context,
                                 MatrixOperation operation, float alpha,
                                 DenseView<const float, 2> matrix,
                                 DenseView<const float, 1> input, float beta,
                                 DenseView<float, 1> output) {
  return internal_dense_cuda::GemvImpl(context, operation, alpha, matrix, input,
                                       beta, output);
}

Result<CompletionEvent> CudaGemv(DenseCudaContext& context,
                                 MatrixOperation operation, double alpha,
                                 DenseView<const double, 2> matrix,
                                 DenseView<const double, 1> input, double beta,
                                 DenseView<double, 1> output) {
  return internal_dense_cuda::GemvImpl(context, operation, alpha, matrix, input,
                                       beta, output);
}

Result<CompletionEvent> CudaGemm(DenseCudaContext& context,
                                 MatrixOperation left_operation,
                                 MatrixOperation right_operation, float alpha,
                                 DenseView<const float, 2> left,
                                 DenseView<const float, 2> right, float beta,
                                 DenseView<float, 2> output) {
  return internal_dense_cuda::GemmImpl(context, left_operation, right_operation,
                                       alpha, left, right, beta, output);
}

Result<CompletionEvent> CudaGemm(DenseCudaContext& context,
                                 MatrixOperation left_operation,
                                 MatrixOperation right_operation, double alpha,
                                 DenseView<const double, 2> left,
                                 DenseView<const double, 2> right, double beta,
                                 DenseView<double, 2> output) {
  return internal_dense_cuda::GemmImpl(context, left_operation, right_operation,
                                       alpha, left, right, beta, output);
}

}  // namespace asc
