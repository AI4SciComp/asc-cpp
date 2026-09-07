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
#include "asc/random/providers/cuda.h"
#include "raw_kernels_internal.h"

namespace asc {
namespace {

Result<void*> ValidateRandomContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA random generation requires a CUDA context");
  }
  const auto& state = internal_core_execution::Access::State(context);
  if (state == nullptr) {
    return Status(ErrorCode::kInvalidState,
                  "The CUDA random context has no provider state");
  }
  void* stream = state->NativeExecutionHandle(Backend::kCuda);
  if (stream == nullptr) {
    return Status(ErrorCode::kUnavailable,
                  "The CUDA random context has no stream");
  }
  return stream;
}

Status ValidateDevicePointer(const void* pointer, std::int32_t device,
                             std::size_t alignment) {
  if (reinterpret_cast<std::uintptr_t>(pointer) % alignment != 0) {
    return Status(ErrorCode::kMemoryAccess,
                  "The CUDA random destination is misaligned");
  }
  cudaPointerAttributes attributes{};
  const cudaError_t error = cudaPointerGetAttributes(&attributes, pointer);
  if (error != cudaSuccess) {
    return internal_core_cuda::CudaStatus(
        error, ErrorCode::kMemoryAccess,
        "CUDA could not inspect the random destination");
  }
  if (attributes.type != cudaMemoryTypeDevice || attributes.device != device) {
    return Status(ErrorCode::kMemoryAccess,
                  "The random destination is not on the context device");
  }
  return Status::Ok();
}

}  // namespace

Result<CudaRandomWordGeneration> CudaFillPhilox4x32(
    const ExecutionContext& context, MutableMemoryView destination,
    std::uint64_t word_count, RandomStream stream,
    RandomSubsequence subsequence, RandomOffset offset) {
  auto next_offset = AdvanceRandomOffset(offset, word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }
  auto native_stream = ValidateRandomContext(context);
  if (!native_stream.ok()) {
    return native_stream.status();
  }
  if (destination.space() != MemorySpace::kDevice) {
    return Status(ErrorCode::kMemoryAccess,
                  "CUDA random words require a device destination");
  }
  if (!destination.valid()) {
    return Status(ErrorCode::kInvalidArgument,
                  "A CUDA random destination span is invalid");
  }
  if (word_count == 0) {
    return CudaRandomWordGeneration{
        .completion = internal_core_execution::Access::MakeCompletedEvent(),
        .next_offset = *next_offset};
  }
  auto bytes = CheckedMultiply<std::uint64_t>(
      word_count, static_cast<std::uint64_t>(sizeof(std::uint32_t)));
  if (!bytes.ok()) {
    return bytes.status();
  }
  if (*bytes > destination.size()) {
    return Status(ErrorCode::kShape,
                  "The CUDA random destination is smaller than word_count");
  }
  if (*bytes >
      UINTPTR_MAX - reinterpret_cast<std::uintptr_t>(destination.data())) {
    return Status(ErrorCode::kOverflow,
                  "The CUDA random destination address span overflows");
  }
  auto guard =
      internal_core_cuda::DeviceGuard::Create(context.device().ordinal);
  if (!guard.ok()) {
    return guard.status();
  }
  Status pointer_status = ValidateDevicePointer(
      destination.data(), context.device().ordinal, alignof(std::uint32_t));
  if (!pointer_status.ok()) {
    return pointer_status;
  }
  Status launch_status = internal_random_cuda::LaunchPhiloxWords(
      *native_stream, static_cast<std::uint32_t*>(destination.data()),
      word_count, stream, subsequence, offset);
  if (!launch_status.ok()) {
    return launch_status;
  }
  auto completion = RecordCudaEvent(context);
  if (!completion.ok()) {
    static_cast<void>(
        cudaStreamSynchronize(static_cast<cudaStream_t>(*native_stream)));
    return completion.status();
  }
  return CudaRandomWordGeneration{.completion = std::move(*completion),
                                  .next_offset = *next_offset};
}

}  // namespace asc
