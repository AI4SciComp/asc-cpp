#include "asc/random/providers/cuda.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include "../../core/cuda/provider_internal.h"
#include "../../core/execution_internal.h"
#include "kernels_internal.h"

namespace asc {
namespace {

Status ValidateAddressSpan(const void* pointer, std::size_t bytes) {
  if (bytes == 0) {
    return Status::Ok();
  }
  if (bytes != 0 && pointer == nullptr) {
    return Status(ErrorCode::kMemoryAccess,
                  "A nonempty CUDA random destination is null");
  }
  if (pointer == nullptr) {
    return Status::Ok();
  }
  const auto address = reinterpret_cast<std::uintptr_t>(pointer);
  if (address % alignof(std::uint32_t) != 0) {
    return Status(ErrorCode::kMemoryAccess,
                  "The CUDA random destination is not uint32_t aligned");
  }
  if (bytes > std::numeric_limits<std::uintptr_t>::max() - address) {
    return Status(ErrorCode::kOverflow,
                  "The CUDA random destination address span overflowed");
  }
  return Status::Ok();
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
                  "The CUDA random destination address span overflowed");
  }
  return Status::Ok();
}

}  // namespace

Result<CudaRandomWordGeneration> CudaFillPhilox4x32(
    const ExecutionContext& context, std::uint32_t* destination,
    std::uint64_t word_count, RandomStream stream,
    RandomSubsequence subsequence, RandomOffset offset) {
  if (context.backend() != Backend::kCuda ||
      context.device().backend != Backend::kCuda) {
    return Status(ErrorCode::kInvalidArgument,
                  "CUDA random generation requires a CUDA context");
  }
  auto next_offset = AdvanceRandomOffset(offset, word_count);
  if (!next_offset.ok()) {
    return next_offset.status();
  }
  if (word_count >
      std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t)) {
    return Status(ErrorCode::kOverflow,
                  "The CUDA random destination byte count overflowed");
  }
  const auto bytes =
      static_cast<std::size_t>(word_count) * sizeof(std::uint32_t);
  const Status address_status = ValidateAddressSpan(destination, bytes);
  if (!address_status.ok()) {
    return address_status;
  }
  const Status memory_status = internal_core_cuda::ValidateCudaMemory(
      context, destination, bytes, MemorySpace::kDevice);
  if (!memory_status.ok()) {
    return memory_status;
  }
  const Status allocation_status =
      ValidateAllocationSpan(context, destination, bytes);
  if (!allocation_status.ok()) {
    return allocation_status;
  }
  if (word_count == 0) {
    return CudaRandomWordGeneration{
        internal_core_execution::CompletionAccess::Completed(), *next_offset};
  }

  auto pending = internal_core_cuda::PendingCudaEvent::Create(context);
  if (!pending.ok()) {
    return pending.status();
  }
  const internal_random_cuda::RawPhiloxPlan plan{
      .destination = destination,
      .word_count = word_count,
      .stream = stream,
      .subsequence = subsequence,
      .offset = offset,
  };
  const Status launch_status =
      internal_random_cuda::LaunchRawPhiloxKernel(context, plan);
  if (!launch_status.ok()) {
    return launch_status;
  }
  auto completion = pending->Record();
  if (!completion.ok()) {
    return completion.status();
  }
  return CudaRandomWordGeneration{std::move(*completion), *next_offset};
}

}  // namespace asc
