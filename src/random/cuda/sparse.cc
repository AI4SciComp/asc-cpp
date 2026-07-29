#include <cuda_runtime_api.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "../../core/cuda/cuda_internal.h"
#include "../../core/execution_internal.h"
#include "asc/core/contracts.h"
#include "asc/core/providers/cuda.h"
#include "asc/random/providers/sparse_cuda.h"
#include "sparse_kernels_internal.h"

namespace asc::internal_random_sparse_cuda {
namespace {

Result<void*> ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA sparse random generation requires a CUDA context");
  }
  const auto& state = internal_core_execution::Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The CUDA sparse random context has no provider state");
  }
  void* stream = state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The CUDA sparse random context has no stream");
  }
  return stream;
}

Status ValidateAllocation(const Buffer& buffer, std::int32_t device,
                          std::size_t alignment) {
  if (buffer.size() == 0) {
    return Status::Ok();
  }
  if (reinterpret_cast<std::uintptr_t>(buffer.data()) % alignment != 0) {
    return Status(ErrorCode::kAllocation,
                  "A CUDA sparse random allocation is misaligned");
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error =
      cudaPointerGetAttributes(&attributes, buffer.data());
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kMemoryAccess,
        "CUDA could not inspect a sparse random allocation");
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "A sparse random allocation is not on the context device");
  }
  return Status::Ok();
}

}  // namespace

Result<GenerationBuffers> GenerateSparseUniform01Erased(
    const ExecutionContext& context, std::span<const extent_t> extents,
    extent_t logical_size, nnz_t exact_count, ElementKind element_kind,
    MemoryResource& resource, RandomStream structure_stream,
    RandomSubsequence structure_subsequence, RandomOffset structure_offset,
    RandomStream value_stream, RandomSubsequence value_subsequence,
    RandomOffset value_offset) {
  auto native_stream = ValidateContext(context);
  if (!native_stream.ok()) {
    return native_stream.status();
  }
  if (logical_size < 0 || exact_count < 0 || exact_count > logical_size) {
    return Status(ErrorCode::kShape,
                  "Sparse exact count is incompatible with the shape");
  }
  if (resource.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA sparse random generation requires a device resource");
  }
  if (structure_stream == value_stream &&
      structure_subsequence == value_subsequence) {
    return Status(
        ErrorCode::kInvalidArgument,
        "Sparse structure and values require distinct random domains");
  }
  auto logical_count = CheckedCast<std::uint64_t>(logical_size);
  if (!logical_count.ok()) {
    return logical_count.status();
  }
  auto selected_count = CheckedCast<std::uint64_t>(exact_count);
  if (!selected_count.ok()) {
    return selected_count.status();
  }
  std::uint64_t structure_words = 0;
  if (exact_count != 0) {
    auto checked = CheckedMultiply<std::uint64_t>(*logical_count, UINT64_C(2));
    if (!checked.ok()) {
      return checked.status();
    }
    structure_words = *checked;
  }
  const std::uint64_t value_words_per_element =
      element_kind == ElementKind::kFloat ? UINT64_C(1) : UINT64_C(2);
  auto value_words =
      CheckedMultiply<std::uint64_t>(*selected_count, value_words_per_element);
  if (!value_words.ok()) {
    return value_words.status();
  }
  auto next_structure_offset =
      AdvanceRandomOffset(structure_offset, structure_words);
  if (!next_structure_offset.ok()) {
    return next_structure_offset.status();
  }
  auto next_value_offset = AdvanceRandomOffset(value_offset, *value_words);
  if (!next_value_offset.ok()) {
    return next_value_offset.status();
  }
  auto coordinate_count =
      CheckedMultiply<std::uint64_t>(*selected_count, extents.size());
  if (!coordinate_count.ok()) {
    return coordinate_count.status();
  }
  auto coordinate_bytes =
      CheckedMultiply<std::uint64_t>(*coordinate_count, sizeof(index_t));
  if (!coordinate_bytes.ok()) {
    return coordinate_bytes.status();
  }
  const std::size_t element_bytes =
      element_kind == ElementKind::kFloat ? sizeof(float) : sizeof(double);
  auto value_bytes =
      CheckedMultiply<std::uint64_t>(*selected_count, element_bytes);
  if (!value_bytes.ok()) {
    return value_bytes.status();
  }
  auto coordinate_size = CheckedCast<std::size_t>(*coordinate_bytes);
  if (!coordinate_size.ok()) {
    return coordinate_size.status();
  }
  auto value_size = CheckedCast<std::size_t>(*value_bytes);
  if (!value_size.ok()) {
    return value_size.status();
  }

  auto coordinates =
      Buffer::Allocate(resource, *coordinate_size, alignof(index_t));
  if (!coordinates.ok()) {
    return coordinates.status();
  }
  auto values = Buffer::Allocate(resource, *value_size, element_bytes);
  if (!values.ok()) {
    return values.status();
  }

  auto guard =
      internal_core_cuda::DeviceGuard::Create(context.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status coordinate_status = ValidateAllocation(
      *coordinates, context.device().ordinal, alignof(index_t));
  if (!coordinate_status.ok()) {
    return coordinate_status;
  }
  Status value_status =
      ValidateAllocation(*values, context.device().ordinal, element_bytes);
  if (!value_status.ok()) {
    return value_status;
  }

  if (exact_count == 0) {
    return GenerationBuffers{
        .coordinates = std::move(*coordinates),
        .values = std::move(*values),
        .completion = internal_core_execution::Access::MakeCompletedEvent(),
        .next_structure_offset = *next_structure_offset,
        .next_value_offset = *next_value_offset};
  }

  SparseDescriptor descriptor;
  descriptor.coordinates = static_cast<index_t*>(coordinates->data());
  descriptor.values = values->data();
  descriptor.element_kind = element_kind;
  descriptor.rank = extents.size();
  descriptor.logical_size = *logical_count;
  descriptor.exact_count = *selected_count;
  Status launch_status = LaunchSparseUniform01(
      *native_stream, descriptor, extents, structure_stream,
      structure_subsequence, structure_offset, value_stream, value_subsequence,
      value_offset);
  if (!launch_status.ok()) {
    return launch_status;
  }
  auto completion = RecordCudaEvent(context);
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*native_stream)));
    return completion.status();
  }
  return GenerationBuffers{.coordinates = std::move(*coordinates),
                           .values = std::move(*values),
                           .completion = std::move(*completion),
                           .next_structure_offset = *next_structure_offset,
                           .next_value_offset = *next_value_offset};
}

}  // namespace asc::internal_random_sparse_cuda
