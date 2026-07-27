#include "asc/random/providers/dense_cuda.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include "../../core/cuda/provider_internal.h"
#include "../../core/execution_internal.h"
#include "dense_kernels_internal.h"

namespace asc {
namespace internal_random_dense_cuda {
namespace {

std::size_t ElementSize(ScalarType scalar_type) {
  return scalar_type == ScalarType::kFloat ? sizeof(float) : sizeof(double);
}

std::uint64_t WordsPerElement(ScalarType scalar_type) {
  return scalar_type == ScalarType::kFloat ? 1U : 2U;
}

Status ValidateScalarType(ScalarType scalar_type) {
  switch (scalar_type) {
    case ScalarType::kFloat:
    case ScalarType::kDouble:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "The CUDA dense random scalar type is invalid");
}

Result<std::size_t> RequiredBytes(const DenseUniformPlan& plan) {
  if (plan.logical_size == 0) {
    return std::size_t{0};
  }
  extent_t maximum_offset = 0;
  for (std::uint8_t dimension = 0; dimension < plan.rank; ++dimension) {
    if (plan.shape[dimension] < 0 || plan.strides[dimension] < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA dense random metadata cannot be negative");
    }
    if (plan.shape[dimension] == 0) {
      return std::size_t{0};
    }
    auto contribution =
        CheckedMultiply(plan.shape[dimension] - 1, plan.strides[dimension]);
    if (!contribution.ok()) {
      return Status(ErrorCode::kOverflow,
                    "CUDA dense random stride arithmetic overflowed");
    }
    auto next = CheckedAdd(maximum_offset, *contribution);
    if (!next.ok()) {
      return Status(ErrorCode::kOverflow,
                    "CUDA dense random span arithmetic overflowed");
    }
    maximum_offset = *next;
  }
  auto span = CheckedAdd(maximum_offset, extent_t{1});
  if (!span.ok()) {
    return Status(ErrorCode::kOverflow,
                  "CUDA dense random element span overflowed");
  }
  auto span_size = CheckedCast<std::size_t>(*span);
  if (!span_size.ok()) {
    return span_size.status();
  }
  auto bytes = CheckedMultiply(*span_size, ElementSize(plan.scalar_type));
  if (!bytes.ok()) {
    return Status(ErrorCode::kOverflow,
                  "CUDA dense random byte span overflowed");
  }
  return *bytes;
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
                  "The CUDA dense random address span overflowed");
  }
  return Status::Ok();
}

}  // namespace

Result<CudaDenseUniform01Generation> LaunchDenseUniform01Opaque(
    const ExecutionContext& context, const void* opaque_plan) {
  if (opaque_plan == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA dense random plan is null");
  }
  const auto& plan = *static_cast<const DenseUniformPlan*>(opaque_plan);
  if (context.backend() != Backend::kCuda ||
      context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA dense random generation requires a CUDA context");
  }
  const Status scalar_status = ValidateScalarType(plan.scalar_type);
  if (!scalar_status.ok()) {
    return scalar_status;
  }
  if (plan.rank > 8 || plan.logical_size < 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "The CUDA dense random rank or logical size is invalid");
  }
  auto logical_size = CheckedCast<std::uint64_t>(plan.logical_size);
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  auto word_count =
      CheckedMultiply(*logical_size, WordsPerElement(plan.scalar_type));
  if (!word_count.ok()) {
    return Status(ErrorCode::kOverflow,
                  "CUDA dense random word consumption overflowed");
  }
  auto next_offset = AdvanceRandomOffset(plan.offset, *word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }
  auto bytes = RequiredBytes(plan);
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (*bytes != 0 && plan.destination != nullptr &&
      reinterpret_cast<std::uintptr_t>(plan.destination) %
              ElementSize(plan.scalar_type) !=
          0) {
    return Status(ErrorCode::kMemoryAccess,
                  "The CUDA dense random destination is misaligned");
  }
  const Status memory_status = internal_core_cuda::ValidateCudaMemory(
      context, plan.destination, *bytes, MemorySpace::kDevice);
  if (!memory_status.ok()) {
    return memory_status;
  }
  const Status allocation_status =
      ValidateAllocationSpan(context, plan.destination, *bytes);
  if (!allocation_status.ok()) {
    return allocation_status;
  }
  if (plan.logical_size == 0) {
    return CudaDenseUniform01Generation{
        internal_core_execution::CompletionAccess::Completed(), *next_offset};
  }

  auto pending = internal_core_cuda::PendingCudaEvent::Create(context);
  if (!pending.ok()) {
    return pending.status();
  }
  const Status launch_status = LaunchDenseUniform01Kernel(context, plan);
  if (!launch_status.ok()) {
    return launch_status;
  }
  auto completion = pending->Record();
  if (!completion.ok()) {
    return completion.status();
  }
  return CudaDenseUniform01Generation{std::move(*completion), *next_offset};
}

}  // namespace internal_random_dense_cuda
}  // namespace asc
