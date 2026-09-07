#include <cublas_v2.h>
#include <driver_types.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/providers/cuda.h"
#include "context_internal.h"

namespace asc::internal_dense_cuda {

Status CublasStatus(cublasStatus_t status, const char* message) {
  ErrorCode code = ErrorCode::kProvider;
  switch (status) {
    case CUBLAS_STATUS_ALLOC_FAILED:
      code = ErrorCode::kAllocation;
      break;
    case CUBLAS_STATUS_ARCH_MISMATCH:
    case CUBLAS_STATUS_NOT_INITIALIZED:
      code = ErrorCode::kUnavailable;
      break;
    case CUBLAS_STATUS_INVALID_VALUE:
      code = ErrorCode::kInvalidArgument;
      break;
    default:
      break;
  }
  return Status(code, message, "cublas", static_cast<std::int64_t>(status));
}

ContextState::~ContextState() {
  auto guard = internal_core_cuda::DeviceGuard::Create(device_);
  if (guard.ok()) {
    static_cast<void>(cublasDestroy(handle_));
  }
}

}  // namespace asc::internal_dense_cuda

namespace asc {

DenseCudaContext::DenseCudaContext(
    ExecutionContext execution_context,
    std::unique_ptr<internal_dense_cuda::ContextState> state) noexcept
    : execution_context_(std::move(execution_context)),
      state_(std::move(state)) {}

DenseCudaContext::DenseCudaContext(DenseCudaContext&& other) noexcept = default;

DenseCudaContext& DenseCudaContext::operator=(
    DenseCudaContext&& other) noexcept {
  if (this != &other) {
    state_.reset();
    execution_context_ = std::move(other.execution_context_);
    state_ = std::move(other.state_);
  }
  return *this;
}

DenseCudaContext::~DenseCudaContext() = default;

Result<DenseCudaContext> DenseCudaContext::Create(
    ExecutionContext execution_context) {
  if (execution_context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "DenseCudaContext requires a CUDA execution context");
  }
  const auto& execution_state =
      internal_core_execution::Access::State(execution_context);
  if (execution_state == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The CUDA execution context has no provider state");
  }
  void* native_stream = execution_state->NativeExecutionHandle(Backend::kCuda);
  if (native_stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The CUDA execution context has no execution stream");
  }

  auto guard = internal_core_cuda::DeviceGuard::Create(
      execution_context.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }

  cublasHandle_t handle = nullptr;
  cublasStatus_t cublas_status = cublasCreate(&handle);
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return internal_dense_cuda::CublasStatus(
        cublas_status, "cuBLAS could not create a provider handle");
  }
  const auto fail = [&](cublasStatus_t status,
                        const char* message) -> Result<DenseCudaContext> {
    static_cast<void>(cublasDestroy(handle));
    return internal_dense_cuda::CublasStatus(status, message);
  };

  cublas_status =
      cublasSetStream(handle, static_cast<cudaStream_t>(native_stream));
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return fail(cublas_status,
                "cuBLAS could not bind the execution-context stream");
  }
  cublas_status = cublasSetPointerMode(handle, CUBLAS_POINTER_MODE_HOST);
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return fail(cublas_status,
                "cuBLAS could not select host scalar pointer mode");
  }
  cublas_status = cublasSetAtomicsMode(handle, CUBLAS_ATOMICS_NOT_ALLOWED);
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return fail(cublas_status,
                "cuBLAS could not disable atomic reduction algorithms");
  }
  const cublasMath_t math_mode =
      execution_context.determinism() == Determinism::kDeterministic
          ? CUBLAS_PEDANTIC_MATH
          : CUBLAS_DEFAULT_MATH;
  cublas_status = cublasSetMathMode(handle, math_mode);
  if (cublas_status != CUBLAS_STATUS_SUCCESS) {
    return fail(cublas_status,
                "cuBLAS could not select the requested math mode");
  }

  auto state = std::make_unique<internal_dense_cuda::ContextState>(
      execution_context.device().ordinal, handle);
  return DenseCudaContext(std::move(execution_context), std::move(state));
}

}  // namespace asc
