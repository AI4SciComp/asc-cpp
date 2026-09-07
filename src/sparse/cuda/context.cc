#include <cusparse.h>
#include <driver_types.h>

#include <cstdint>
#include <memory>
#include <utility>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/sparse/providers/cuda.h"
#include "context_internal.h"

namespace asc::internal_sparse_cuda {

Status CusparseStatus(cusparseStatus_t status, const char* message) {
  ErrorCode code = ErrorCode::kProvider;
  switch (status) {
    case CUSPARSE_STATUS_ALLOC_FAILED:
      code = ErrorCode::kAllocation;
      break;
    case CUSPARSE_STATUS_ARCH_MISMATCH:
    case CUSPARSE_STATUS_NOT_INITIALIZED:
      code = ErrorCode::kUnavailable;
      break;
    case CUSPARSE_STATUS_INVALID_VALUE:
      code = ErrorCode::kInvalidArgument;
      break;
    case CUSPARSE_STATUS_INSUFFICIENT_RESOURCES:
      code = ErrorCode::kAllocation;
      break;
    default:
      break;
  }
  return Status(code, message, "cusparse", static_cast<std::int64_t>(status));
}

ContextState::~ContextState() {
  auto guard = internal_core_cuda::DeviceGuard::Create(device_);
  if (guard.ok()) {
    static_cast<void>(cusparseDestroy(handle_));
  }
}

}  // namespace asc::internal_sparse_cuda

namespace asc {

SparseCudaContext::SparseCudaContext(
    ExecutionContext execution_context,
    std::unique_ptr<internal_sparse_cuda::ContextState> state) noexcept
    : execution_context_(std::move(execution_context)),
      state_(std::move(state)) {}

SparseCudaContext::SparseCudaContext(SparseCudaContext&& other) noexcept =
    default;

SparseCudaContext& SparseCudaContext::operator=(
    SparseCudaContext&& other) noexcept {
  if (this != &other) {
    state_.reset();
    execution_context_ = std::move(other.execution_context_);
    state_ = std::move(other.state_);
  }
  return *this;
}

SparseCudaContext::~SparseCudaContext() = default;

Result<SparseCudaContext> SparseCudaContext::Create(
    ExecutionContext execution_context) {
  if (execution_context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "SparseCudaContext requires a CUDA execution context");
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
  cusparseHandle_t handle = nullptr;
  cusparseStatus_t status = cusparseCreate(&handle);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return internal_sparse_cuda::CusparseStatus(
        status, "cuSPARSE could not create a provider handle");
  }
  const auto fail = [&](cusparseStatus_t failed,
                        const char* message) -> Result<SparseCudaContext> {
    static_cast<void>(cusparseDestroy(handle));
    return internal_sparse_cuda::CusparseStatus(failed, message);
  };
  status = cusparseSetStream(handle, static_cast<cudaStream_t>(native_stream));
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return fail(status, "cuSPARSE could not bind the execution-context stream");
  }
  status = cusparseSetPointerMode(handle, CUSPARSE_POINTER_MODE_HOST);
  if (status != CUSPARSE_STATUS_SUCCESS) {
    return fail(status, "cuSPARSE could not select host pointer mode");
  }
  auto state = std::make_unique<internal_sparse_cuda::ContextState>(
      execution_context.device().ordinal, handle);
  return SparseCudaContext(std::move(execution_context), std::move(state));
}

}  // namespace asc
