#include "asc/sparse/providers/cuda.h"

#include <cuda_runtime_api.h>
#include <cusparse.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "../../core/cuda/provider_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/providers/cuda.h"
#include "kernels_internal.h"

namespace asc {
namespace internal_sparse_cuda {
namespace {

Status CusparseStatus(cusparseStatus_t status, ErrorCode code,
                      const char* operation) {
  if (status == CUSPARSE_STATUS_SUCCESS) {
    return Status::Ok();
  }
  return Status(code,
                std::string(operation) + ": " + cusparseGetErrorString(status),
                "cusparse", static_cast<std::int64_t>(status));
}

Status CudaStatus(cudaError_t error, const char* operation) {
  if (error == cudaSuccess) {
    return Status::Ok();
  }
  static_cast<void>(cudaGetLastError());
  return Status(ErrorCode::kProvider,
                std::string(operation) + ": " + cudaGetErrorString(error),
                "cuda", static_cast<std::int64_t>(error));
}

std::size_t ElementSize(ScalarType type) {
  return type == ScalarType::kFloat ? sizeof(float) : sizeof(double);
}

cudaDataType DataType(ScalarType type) {
  return type == ScalarType::kFloat ? CUDA_R_32F : CUDA_R_64F;
}

Status ValidateScalarType(ScalarType type) {
  switch (type) {
    case ScalarType::kFloat:
    case ScalarType::kDouble:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "A CUDA sparse scalar type is invalid");
}

Result<std::size_t> CheckedByteCount(std::uint64_t count,
                                     std::size_t element_size,
                                     const char* diagnostic) {
  if (count > std::numeric_limits<std::size_t>::max() / element_size) {
    return Status(ErrorCode::kOverflow, diagnostic);
  }
  return static_cast<std::size_t>(count) * element_size;
}

Status ValidateAllocationSpan(const ExecutionContext& context,
                              const void* pointer, std::size_t bytes,
                              std::size_t alignment) {
  if (bytes == 0) {
    return Status::Ok();
  }
  if (pointer == nullptr ||
      reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) {
    return Status(ErrorCode::kMemoryAccess,
                  "A CUDA sparse span is null or misaligned");
  }
  const Status memory_status = internal_core_cuda::ValidateCudaMemory(
      context, pointer, bytes, MemorySpace::kDevice);
  if (!memory_status.ok()) {
    return memory_status;
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(ErrorCode::kOverflow,
                  "A CUDA sparse address span overflowed");
  }
  return Status::Ok();
}

bool Overlap(const void* left, std::size_t left_bytes, const void* right,
             std::size_t right_bytes) {
  return internal_sparse_coordinate::ByteSpansOverlap(left, left_bytes, right,
                                                      right_bytes);
}

Status Drain(const ExecutionContext& context) {
  auto stream = internal_core_cuda::CudaStreamHandle(context);
  if (!stream.ok()) {
    return stream.status();
  }
  return CudaStatus(
      cudaStreamSynchronize(reinterpret_cast<cudaStream_t>(*stream)),
      "cudaStreamSynchronize after sparse provider failure");
}

}  // namespace

class SparseCudaContextState {
 public:
  explicit SparseCudaContextState(cusparseHandle_t handle) noexcept
      : handle_(handle) {}
  ~SparseCudaContextState() = default;

  [[nodiscard]] cusparseHandle_t handle() const noexcept { return handle_; }
  [[nodiscard]] std::mutex& mutex() noexcept { return mutex_; }

 private:
  cusparseHandle_t handle_;
  std::mutex mutex_;
};

class SparseCudaContextAccess {
 public:
  static SparseCudaContextState* State(
      const SparseCudaContext& context) noexcept {
    return context.state_.get();
  }
};

namespace {

Status ValidateContext(const SparseCudaContext& context) {
  if (context.execution_context().backend() != Backend::kCuda ||
      context.execution_context().device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "A sparse CUDA operation requires a CUDA context");
  }
  if (SparseCudaContextAccess::State(context) == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "A moved-from SparseCudaContext is invalid");
  }
  return Status::Ok();
}

Status ValidateSpmv(const SparseCudaContext& context, const SpmvPlan& plan,
                    std::size_t& outer_bytes, std::size_t& inner_bytes,
                    std::size_t& value_bytes, std::size_t& input_bytes,
                    std::size_t& output_bytes) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status scalar_status = ValidateScalarType(plan.scalar_type);
  if (!scalar_status.ok()) {
    return scalar_status;
  }
  if (plan.rows < 0 || plan.columns < 0 || plan.nnz < 0 ||
      plan.input_stride <= 0 || plan.output_stride <= 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA SpMV metadata cannot be negative");
  }
  auto outer_count =
      CheckedAdd(static_cast<std::uint64_t>(plan.rows), std::uint64_t{1});
  if (!outer_count.ok()) {
    return outer_count.status();
  }
  auto outer = CheckedByteCount(*outer_count, sizeof(nnz_t),
                                "CUDA CSR outer bytes overflowed");
  auto inner =
      CheckedByteCount(static_cast<std::uint64_t>(plan.nnz), sizeof(index_t),
                       "CUDA CSR inner bytes overflowed");
  auto values = CheckedByteCount(static_cast<std::uint64_t>(plan.nnz),
                                 ElementSize(plan.scalar_type),
                                 "CUDA CSR value bytes overflowed");
  std::uint64_t input_span = 0;
  if (plan.columns != 0) {
    auto input_offset =
        CheckedMultiply(static_cast<std::uint64_t>(plan.columns - 1),
                        static_cast<std::uint64_t>(plan.input_stride));
    if (!input_offset.ok()) {
      return Status(ErrorCode::kOverflow,
                    "CUDA SpMV input stride span overflowed");
    }
    auto checked_span = CheckedAdd(*input_offset, std::uint64_t{1});
    if (!checked_span.ok()) {
      return checked_span.status();
    }
    input_span = *checked_span;
  }
  std::uint64_t output_span = 0;
  if (plan.rows != 0) {
    auto output_offset =
        CheckedMultiply(static_cast<std::uint64_t>(plan.rows - 1),
                        static_cast<std::uint64_t>(plan.output_stride));
    if (!output_offset.ok()) {
      return Status(ErrorCode::kOverflow,
                    "CUDA SpMV output stride span overflowed");
    }
    auto checked_span = CheckedAdd(*output_offset, std::uint64_t{1});
    if (!checked_span.ok()) {
      return checked_span.status();
    }
    output_span = *checked_span;
  }
  auto input = CheckedByteCount(input_span, ElementSize(plan.scalar_type),
                                "CUDA SpMV input bytes overflowed");
  auto output = CheckedByteCount(output_span, ElementSize(plan.scalar_type),
                                 "CUDA SpMV output bytes overflowed");
  if (!outer.ok() || !inner.ok() || !values.ok() || !input.ok() ||
      !output.ok()) {
    return Status(ErrorCode::kOverflow, "A CUDA SpMV byte span overflowed");
  }
  outer_bytes = *outer;
  inner_bytes = *inner;
  value_bytes = *values;
  input_bytes = *input;
  output_bytes = *output;
  const ExecutionContext& execution = context.execution_context();
  for (const auto& item :
       {std::pair{static_cast<const void*>(plan.outer_offsets), outer_bytes},
        std::pair{static_cast<const void*>(plan.inner_indices), inner_bytes},
        std::pair{plan.values, value_bytes}, std::pair{plan.input, input_bytes},
        std::pair{static_cast<const void*>(plan.output), output_bytes}}) {
    const Status status = ValidateAllocationSpan(
        execution, item.first, item.second,
        item.first == plan.values || item.first == plan.input ||
                item.first == plan.output
            ? ElementSize(plan.scalar_type)
            : alignof(std::int64_t));
    if (!status.ok()) {
      return status;
    }
  }
  if (Overlap(plan.output, output_bytes, plan.input, input_bytes) ||
      Overlap(plan.output, output_bytes, plan.values, value_bytes) ||
      Overlap(plan.output, output_bytes, plan.outer_offsets, outer_bytes) ||
      Overlap(plan.output, output_bytes, plan.inner_indices, inner_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA SpMV output overlaps an input");
  }
  return Status::Ok();
}

struct CusparseDescriptors {
  cusparseSpMatDescr_t matrix = nullptr;
  cusparseDnVecDescr_t input = nullptr;
  cusparseDnVecDescr_t output = nullptr;

  ~CusparseDescriptors() {
    if (output != nullptr) {
      static_cast<void>(cusparseDestroyDnVec(output));
    }
    if (input != nullptr) {
      static_cast<void>(cusparseDestroyDnVec(input));
    }
    if (matrix != nullptr) {
      static_cast<void>(cusparseDestroySpMat(matrix));
    }
  }
};

Result<std::size_t> CusparseWorkspace(const SparseCudaContext& context,
                                      const SpmvPlan& plan,
                                      CusparseDescriptors& descriptors) {
  const cudaDataType data_type = DataType(plan.scalar_type);
  cusparseStatus_t status = cusparseCreateCsr(
      &descriptors.matrix, plan.rows, plan.columns, plan.nnz,
      const_cast<nnz_t*>(plan.outer_offsets),
      const_cast<index_t*>(plan.inner_indices), const_cast<void*>(plan.values),
      CUSPARSE_INDEX_64I, CUSPARSE_INDEX_64I, CUSPARSE_INDEX_BASE_ZERO,
      data_type);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(status, ErrorCode::kProvider, "cusparseCreateCsr");
  }
  status = cusparseCreateDnVec(&descriptors.input, plan.columns,
                               const_cast<void*>(plan.input), data_type);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(status, ErrorCode::kProvider, "cusparseCreateDnVec");
  }
  status = cusparseCreateDnVec(&descriptors.output, plan.rows, plan.output,
                               data_type);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(status, ErrorCode::kProvider, "cusparseCreateDnVec");
  }
  std::size_t workspace_bytes = 0;
  if (plan.scalar_type == ScalarType::kFloat) {
    const float alpha = static_cast<float>(plan.alpha);
    const float beta = static_cast<float>(plan.beta);
    status = cusparseSpMV_bufferSize(
        SparseCudaContextAccess::State(context)->handle(),
        CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, descriptors.matrix,
        descriptors.input, &beta, descriptors.output, data_type,
        CUSPARSE_SPMV_CSR_ALG2, &workspace_bytes);
  } else {
    const double alpha = plan.alpha;
    const double beta = plan.beta;
    status = cusparseSpMV_bufferSize(
        SparseCudaContextAccess::State(context)->handle(),
        CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, descriptors.matrix,
        descriptors.input, &beta, descriptors.output, data_type,
        CUSPARSE_SPMV_CSR_ALG2, &workspace_bytes);
  }
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return CusparseStatus(status, ErrorCode::kProvider,
                          "cusparseSpMV_bufferSize");
  }
  return workspace_bytes;
}

}  // namespace

Result<CompletionEvent> CloneCsrOpaque(const SparseCudaContext& context,
                                       const void* opaque_plan) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA CSR clone plan is null");
  }
  const auto& plan = *static_cast<const ClonePlan*>(opaque_plan);
  std::optional<CompletionEvent> last;
  const auto copy = [&](void* destination, const void* source,
                        std::size_t bytes) -> Status {
    if (bytes == 0) {
      return Status::Ok();
    }
    auto event =
        CopyBytes(context.execution_context(),
                  MutableMemoryView(destination, bytes, MemorySpace::kDevice),
                  ConstMemoryView(source, bytes, MemorySpace::kHost), bytes);
    if (!event.ok()) {
      if (last.has_value()) {
        static_cast<void>(last->Wait());
      }
      return event.status();
    }
    last = std::move(*event);
    return Status::Ok();
  };
  Status status = copy(plan.destination_outer_offsets,
                       plan.source_outer_offsets, plan.outer_bytes);
  if (status.ok()) {
    status = copy(plan.destination_inner_indices, plan.source_inner_indices,
                  plan.inner_bytes);
  }
  if (status.ok()) {
    status =
        copy(plan.destination_values, plan.source_values, plan.value_bytes);
  }
  if (!status.ok()) {
    return status;
  }
  auto completion = RecordCudaEvent(context.execution_context());
  if (!completion.ok()) {
    if (last.has_value()) {
      static_cast<void>(last->Wait());
    }
    return completion.status();
  }
  return completion;
}

Result<std::size_t> QuerySpmvWorkspaceOpaque(const SparseCudaContext& context,
                                             const void* opaque_plan) {
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument, "The CUDA SpMV plan is null");
  }
  const auto& plan = *static_cast<const SpmvPlan*>(opaque_plan);
  std::size_t outer_bytes = 0;
  std::size_t inner_bytes = 0;
  std::size_t value_bytes = 0;
  std::size_t input_bytes = 0;
  std::size_t output_bytes = 0;
  const Status validation =
      ValidateSpmv(context, plan, outer_bytes, inner_bytes, value_bytes,
                   input_bytes, output_bytes);
  if (!validation.ok()) {
    return validation;
  }
  if (plan.input_stride != 1 || plan.output_stride != 1 || plan.rows == 0) {
    return std::size_t{0};
  }
  std::lock_guard<std::mutex> lock(
      SparseCudaContextAccess::State(context)->mutex());
  auto guard =
      internal_core_cuda::CudaDeviceGuard::Create(context.execution_context());
  if (!guard.ok()) {
    return guard.status();
  }
  CusparseDescriptors descriptors;
  return CusparseWorkspace(context, plan, descriptors);
}

Result<CompletionEvent> LaunchSpmvOpaque(const SparseCudaContext& context,
                                         const void* opaque_plan) {
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument, "The CUDA SpMV plan is null");
  }
  const auto& plan = *static_cast<const SpmvPlan*>(opaque_plan);
  std::size_t outer_bytes = 0;
  std::size_t inner_bytes = 0;
  std::size_t value_bytes = 0;
  std::size_t input_bytes = 0;
  std::size_t output_bytes = 0;
  const Status validation =
      ValidateSpmv(context, plan, outer_bytes, inner_bytes, value_bytes,
                   input_bytes, output_bytes);
  if (!validation.ok()) {
    return validation;
  }
  if (plan.rows == 0) {
    return internal_core_execution::CompletionAccess::Completed();
  }
  if (plan.input_stride != 1 || plan.output_stride != 1) {
    auto pending = internal_core_cuda::PendingCudaEvent::Create(
        context.execution_context());
    if (!pending.ok()) {
      return pending.status();
    }
    const Status launch =
        LaunchStridedSpmvKernel(context.execution_context(), plan);
    if (!launch.ok()) {
      return launch;
    }
    return pending->Record();
  }

  std::lock_guard<std::mutex> lock(
      SparseCudaContextAccess::State(context)->mutex());
  auto guard =
      internal_core_cuda::CudaDeviceGuard::Create(context.execution_context());
  if (!guard.ok()) {
    return guard.status();
  }
  CusparseDescriptors descriptors;
  auto required = CusparseWorkspace(context, plan, descriptors);
  if (!required.ok()) {
    return required.status();
  }
  if (plan.workspace.size() < *required ||
      (*required != 0 && plan.workspace.space() != MemorySpace::kDevice)) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA SpMV workspace is too small or misplaced");
  }
  if (*required != 0) {
    const Status workspace_status = ValidateAllocationSpan(
        context.execution_context(), plan.workspace.data(), *required,
        alignof(std::max_align_t));
    if (!workspace_status.ok()) {
      return workspace_status;
    }
    if (Overlap(plan.workspace.data(), *required, plan.output, output_bytes) ||
        Overlap(plan.workspace.data(), *required, plan.input, input_bytes) ||
        Overlap(plan.workspace.data(), *required, plan.values, value_bytes) ||
        Overlap(plan.workspace.data(), *required, plan.outer_offsets,
                outer_bytes) ||
        Overlap(plan.workspace.data(), *required, plan.inner_indices,
                inner_bytes)) {
      return Status(ErrorCode::kInvalidArgument,
                    "The CUDA SpMV workspace overlaps an operand");
    }
  }
  auto pending =
      internal_core_cuda::PendingCudaEvent::Create(context.execution_context());
  if (!pending.ok()) {
    return pending.status();
  }
  // Isolate this submission from an unrelated sticky Runtime error. The
  // pending event will attribute only errors produced after this reset.
  static_cast<void>(cudaGetLastError());
  cusparseStatus_t status;
  if (plan.scalar_type == ScalarType::kFloat) {
    const float alpha = static_cast<float>(plan.alpha);
    const float beta = static_cast<float>(plan.beta);
    status = cusparseSpMV(SparseCudaContextAccess::State(context)->handle(),
                          CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha,
                          descriptors.matrix, descriptors.input, &beta,
                          descriptors.output, CUDA_R_32F,
                          CUSPARSE_SPMV_CSR_ALG2, plan.workspace.data());
  } else {
    const double alpha = plan.alpha;
    const double beta = plan.beta;
    status = cusparseSpMV(SparseCudaContextAccess::State(context)->handle(),
                          CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha,
                          descriptors.matrix, descriptors.input, &beta,
                          descriptors.output, CUDA_R_64F,
                          CUSPARSE_SPMV_CSR_ALG2, plan.workspace.data());
  }
  if (status != CUSPARSE_STATUS_SUCCESS) {
    static_cast<void>(Drain(context.execution_context()));
    return CusparseStatus(status, ErrorCode::kProvider, "cusparseSpMV");
  }
  return pending->Record();
}

Result<CompletionEvent> LaunchPointwiseOpaque(const SparseCudaContext& context,
                                              const void* opaque_plan) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA sparse pointwise plan is null");
  }
  const auto& plan = *static_cast<const PointwisePlan*>(opaque_plan);
  const Status scalar_status = ValidateScalarType(plan.scalar_type);
  if (!scalar_status.ok()) {
    return scalar_status;
  }
  if (plan.rank > 8 || plan.nnz < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA sparse pointwise metadata is invalid");
  }
  auto value_bytes = CheckedByteCount(static_cast<std::uint64_t>(plan.nnz),
                                      ElementSize(plan.scalar_type),
                                      "CUDA sparse pointwise bytes overflowed");
  if (!value_bytes.ok()) {
    return value_bytes.status();
  }
  const Status destination_status =
      ValidateAllocationSpan(context.execution_context(), plan.destination,
                             *value_bytes, ElementSize(plan.scalar_type));
  if (!destination_status.ok()) {
    return destination_status;
  }
  for (const PointwiseOperand* operand : {&plan.left, &plan.right}) {
    if (operand->is_scalar || operand->values == nullptr) {
      continue;
    }
    const Status operand_status =
        ValidateAllocationSpan(context.execution_context(), operand->values,
                               *value_bytes, ElementSize(plan.scalar_type));
    if (!operand_status.ok()) {
      return operand_status;
    }
  }
  if (plan.no_op || plan.nnz == 0) {
    return internal_core_execution::CompletionAccess::Completed();
  }
  auto pending =
      internal_core_cuda::PendingCudaEvent::Create(context.execution_context());
  if (!pending.ok()) {
    return pending.status();
  }
  const Status launch =
      LaunchSparsePointwiseKernel(context.execution_context(), plan);
  if (!launch.ok()) {
    return launch;
  }
  return pending->Record();
}

}  // namespace internal_sparse_cuda

SparseCudaContext::SparseCudaContext(
    ExecutionContext execution_context,
    std::unique_ptr<internal_sparse_cuda::SparseCudaContextState> state)
    : execution_context_(std::move(execution_context)),
      state_(std::move(state)) {}

SparseCudaContext::SparseCudaContext(SparseCudaContext&& other) noexcept =
    default;
SparseCudaContext& SparseCudaContext::operator=(
    SparseCudaContext&& other) noexcept {
  if (this != &other) {
    SparseCudaContext temporary(std::move(other));
    std::swap(execution_context_, temporary.execution_context_);
    state_.swap(temporary.state_);
  }
  return *this;
}

SparseCudaContext::~SparseCudaContext() {
  if (state_ == nullptr) {
    return;
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(execution_context_);
  if (!guard.ok()) {
    state_.release();
    return;
  }
  const cusparseHandle_t handle = state_->handle();
  state_.reset();
  static_cast<void>(cusparseDestroy(handle));
}

Result<SparseCudaContext> SparseCudaContext::Create(
    ExecutionContext execution_context) {
  if (execution_context.backend() != Backend::kCuda ||
      execution_context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "SparseCudaContext requires a CUDA execution context");
  }
  auto guard = internal_core_cuda::CudaDeviceGuard::Create(execution_context);
  if (!guard.ok()) {
    return guard.status();
  }
  cusparseHandle_t handle = nullptr;
  cusparseStatus_t status = cusparseCreate(&handle);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return internal_sparse_cuda::CusparseStatus(status, ErrorCode::kUnavailable,
                                                "cusparseCreate");
  }
  auto stream = internal_core_cuda::CudaStreamHandle(execution_context);
  if (!stream.ok()) {
    static_cast<void>(cusparseDestroy(handle));
    return stream.status();
  }
  status = cusparseSetStream(handle, reinterpret_cast<cudaStream_t>(*stream));
  if (status != CUSPARSE_STATUS_SUCCESS) {
    static_cast<void>(cusparseDestroy(handle));
    return internal_sparse_cuda::CusparseStatus(status, ErrorCode::kProvider,
                                                "cusparseSetStream");
  }
  return SparseCudaContext(
      std::move(execution_context),
      std::make_unique<internal_sparse_cuda::SparseCudaContextState>(handle));
}

const ExecutionContext& SparseCudaContext::execution_context() const noexcept {
  return execution_context_;
}

}  // namespace asc
