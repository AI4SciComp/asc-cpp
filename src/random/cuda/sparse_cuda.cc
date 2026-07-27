#include "asc/random/providers/sparse_cuda.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "../../core/cuda/provider_internal.h"
#include "../../core/execution_internal.h"
#include "sparse_kernels_internal.h"

namespace asc {
namespace internal_random_sparse_cuda {
namespace {

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
                "The CUDA sparse random scalar type is invalid");
}

Result<std::size_t> CheckedBytes(std::uint64_t count, std::size_t element_size,
                                 const char* diagnostic) {
  if (count > std::numeric_limits<std::size_t>::max() / element_size) {
    return Status(ErrorCode::kOverflow, diagnostic);
  }
  return static_cast<std::size_t>(count) * element_size;
}

Status ValidateAllocationSpan(const ExecutionContext& context,
                              const void* pointer, std::size_t bytes) {
  static_cast<void>(context);
  if (bytes == 0) {
    return Status::Ok();
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return Status(ErrorCode::kOverflow,
                  "A CUDA sparse random address span overflowed");
  }
  return Status::Ok();
}

}  // namespace

Result<CompletionEvent> LaunchSparseUniform01Opaque(
    const ExecutionContext& context, const void* opaque_plan) {
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA sparse random plan is null");
  }
  const auto& plan = *static_cast<const SparseUniformPlan*>(opaque_plan);
  if (context.backend() != Backend::kCuda ||
      context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse random generation requires a CUDA context");
  }
  const Status scalar_status = ValidateScalarType(plan.scalar_type);
  if (!scalar_status.ok()) {
    return scalar_status;
  }
  if (plan.rank > 8 || plan.logical_size < 0 || plan.exact_count < 0 ||
      plan.exact_count > plan.logical_size) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA sparse random plan metadata is invalid");
  }
  auto count = CheckedCast<std::uint64_t>(plan.exact_count);
  if (!count.ok()) {
    return count.status();
  }
  auto coordinate_count =
      CheckedMultiply(*count, static_cast<std::uint64_t>(plan.rank));
  if (!coordinate_count.ok()) {
    return Status(ErrorCode::kOverflow,
                  "The CUDA sparse random coordinate count overflowed");
  }
  auto coordinate_bytes =
      CheckedBytes(*coordinate_count, sizeof(index_t),
                   "The CUDA sparse random coordinate bytes overflowed");
  if (!coordinate_bytes.ok()) {
    return coordinate_bytes.status();
  }
  auto value_bytes =
      CheckedBytes(*count, ElementSize(plan.scalar_type),
                   "The CUDA sparse random value bytes overflowed");
  if (!value_bytes.ok()) {
    return value_bytes.status();
  }
  const Status coordinate_status = internal_core_cuda::ValidateCudaMemory(
      context, plan.coordinates, *coordinate_bytes, MemorySpace::kDevice);
  if (!coordinate_status.ok()) {
    return coordinate_status;
  }
  const Status value_status = internal_core_cuda::ValidateCudaMemory(
      context, plan.values, *value_bytes, MemorySpace::kDevice);
  if (!value_status.ok()) {
    return value_status;
  }
  const Status coordinate_allocation_status =
      ValidateAllocationSpan(context, plan.coordinates, *coordinate_bytes);
  if (!coordinate_allocation_status.ok()) {
    return coordinate_allocation_status;
  }
  const Status value_allocation_status =
      ValidateAllocationSpan(context, plan.values, *value_bytes);
  if (!value_allocation_status.ok()) {
    return value_allocation_status;
  }
  if (internal_sparse_coordinate::ByteSpansOverlap(
          plan.coordinates, *coordinate_bytes, plan.values, *value_bytes)) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse random output spans overlap");
  }
  if (plan.exact_count == 0) {
    return internal_core_execution::CompletionAccess::Completed();
  }

  auto pending = internal_core_cuda::PendingCudaEvent::Create(context);
  if (!pending.ok()) {
    return pending.status();
  }
  const Status launch_status = LaunchSparseUniform01Kernel(context, plan);
  if (!launch_status.ok()) {
    return launch_status;
  }
  return pending->Record();
}

}  // namespace internal_random_sparse_cuda
}  // namespace asc
