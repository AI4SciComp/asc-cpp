#include <cuda_runtime_api.h>
#include <driver_types.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/random/engine.h"
#include "asc/random/providers/dense_cuda.h"
#include "dense_kernels_internal.h"

namespace asc::internal_random_dense_cuda {
namespace {

Result<void*> ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA Dense random fill requires a CUDA context");
  }
  const auto& state = internal_core_execution::Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The CUDA Dense random context has no provider state");
  }
  void* stream = state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The CUDA Dense random context has no stream");
  }
  return stream;
}

std::size_t ElementBytes(ElementKind kind) {
  return kind == ElementKind::kFloat ? sizeof(float) : sizeof(double);
}

// Validation remains a single ordered pass so descriptor failures have stable
// precedence across all ranks.
// NOLINTNEXTLINE(readability-function-size)
Status ValidateDescriptor(const ViewDescriptor& descriptor) {
  if (descriptor.rank > 8) {
    return Status(ErrorCode::kUnsupported,
                  "CUDA Dense random fill supports ranks through eight");
  }
  if (descriptor.memory_space != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA Dense random fill requires device storage");
  }
  extent_t logical_size = 1;
  bool empty = false;
  stride_t maximum_offset = 0;
  for (std::size_t dimension = 0; dimension < descriptor.rank; ++dimension) {
    const extent_t extent = descriptor.extents[dimension];
    const stride_t stride = descriptor.strides[dimension];
    if (extent < 0 || stride < 0) {
      return Status(ErrorCode::kInvalidArgument,
                    "CUDA Dense random extents and strides cannot be negative");
    }
    empty = empty || extent == 0;
    if (!empty) {
      auto product = CheckedMultiply(logical_size, extent);
      if (!product.ok()) {
        return product.status();
      }
      logical_size = *product;
    }
    if (extent > 0) {
      auto contribution = CheckedMultiply<stride_t>(extent - 1, stride);
      if (!contribution.ok()) {
        return contribution.status();
      }
      auto next_offset = CheckedAdd(maximum_offset, *contribution);
      if (!next_offset.ok()) {
        return next_offset.status();
      }
      maximum_offset = *next_offset;
    }
  }
  if (empty) {
    logical_size = 0;
  }
  if (logical_size != descriptor.logical_size) {
    return Status(ErrorCode::kShape,
                  "CUDA Dense random logical size is inconsistent");
  }
  std::size_t expected_span = 0;
  if (logical_size != 0) {
    auto span = CheckedAdd(maximum_offset, stride_t{1});
    if (!span.ok()) {
      return span.status();
    }
    auto converted = CheckedCast<std::size_t>(*span);
    if (!converted.ok()) {
      return converted.status();
    }
    expected_span = *converted;
  }
  if (expected_span != descriptor.required_span_size) {
    return Status(ErrorCode::kShape,
                  "CUDA Dense random storage span is inconsistent");
  }
  auto bytes =
      CheckedMultiply(expected_span, ElementBytes(descriptor.element_kind));
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (*bytes != 0 && descriptor.data == nullptr) {
    return Status(ErrorCode::kInvalidArgument,
                  "A nonempty CUDA Dense random destination cannot be null");
  }
  if (*bytes != 0 && reinterpret_cast<std::uintptr_t>(descriptor.data) %
                             ElementBytes(descriptor.element_kind) !=
                         0) {
    return Status(ErrorCode::kMemoryAccess,
                  "The CUDA Dense random destination is misaligned");
  }
  if (*bytes != 0 && *bytes > UINTPTR_MAX - reinterpret_cast<std::uintptr_t>(
                                                descriptor.data)) {
    return Status(ErrorCode::kOverflow,
                  "The CUDA Dense random destination address span overflows");
  }
  return Status::Ok();
}

Status ValidatePointer(const ViewDescriptor& descriptor, std::int32_t device) {
  if (descriptor.logical_size == 0) {
    return Status::Ok();
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error =
      cudaPointerGetAttributes(&attributes, descriptor.data);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kMemoryAccess,
        "CUDA could not inspect the Dense random destination");
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "The Dense random destination is not on the context device");
  }
  return Status::Ok();
}

}  // namespace

Result<Generation> FillDenseUniform01Erased(const ExecutionContext& context,
                                            ViewDescriptor destination,
                                            RandomStream stream,
                                            RandomSubsequence subsequence,
                                            RandomOffset offset) {
  auto native_stream = ValidateContext(context);
  if (!native_stream.ok()) {
    return native_stream.status();
  }
  Status descriptor_status = ValidateDescriptor(destination);
  if (!descriptor_status.ok()) {
    return descriptor_status;
  }
  constexpr std::uint64_t kFloatWords = 1;
  constexpr std::uint64_t kDoubleWords = 2;
  const std::uint64_t words_per_element =
      destination.element_kind == ElementKind::kFloat ? kFloatWords
                                                      : kDoubleWords;
  auto logical_size = CheckedCast<std::uint64_t>(destination.logical_size);
  if (!logical_size.ok()) {
    return logical_size.status();
  }
  auto word_count =
      CheckedMultiply<std::uint64_t>(*logical_size, words_per_element);
  if (!word_count.ok()) {
    return word_count.status();
  }
  auto next_offset = AdvanceRandomOffset(offset, *word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }
  if (destination.logical_size == 0) {
    return Generation{
        .completion = internal_core_execution::Access::MakeCompletedEvent(),
        .next_offset = *next_offset};
  }
  auto guard =
      internal_core_cuda::DeviceGuard::Create(context.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status pointer_status =
      ValidatePointer(destination, context.device().ordinal);
  if (!pointer_status.ok()) {
    return pointer_status;
  }
  Status launch_status = LaunchDenseUniform01(*native_stream, destination,
                                              stream, subsequence, offset);
  if (!launch_status.ok()) {
    return launch_status;
  }
  auto completion = RecordCudaEvent(context);
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*native_stream)));
    return completion.status();
  }
  return Generation{.completion = std::move(*completion),
                    .next_offset = *next_offset};
}

}  // namespace asc::internal_random_dense_cuda
