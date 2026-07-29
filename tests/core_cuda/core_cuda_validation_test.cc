#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "test_support.h"

int main() {
  if (asc_core_cuda_test::ForceNoCudaDevice()) {
    return asc_core_cuda_test::kSkipReturnCode;
  }
  asc_core_cuda_test::TestContext test;

  auto count = asc::CudaDeviceCount();
  ASC_CORE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }

  auto negative_context = asc::CreateCudaExecutionContext(-1);
  ASC_CORE_CUDA_CHECK(test, !negative_context.ok());
  if (!negative_context.ok()) {
    ASC_CORE_CUDA_EQ(test, negative_context.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  }
  auto past_end_context = asc::CreateCudaExecutionContext(*count);
  ASC_CORE_CUDA_CHECK(test, !past_end_context.ok());

  auto host_resource =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kHost);
  ASC_CORE_CUDA_CHECK(test, !host_resource.ok());
  if (!host_resource.ok()) {
    ASC_CORE_CUDA_EQ(test, host_resource.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  }
  auto negative_resource =
      asc::CudaMemoryResource::Create(-1, asc::MemorySpace::kDevice);
  ASC_CORE_CUDA_CHECK(test, !negative_resource.ok());
  auto past_end_resource =
      asc::CudaMemoryResource::Create(*count, asc::MemorySpace::kDevice);
  ASC_CORE_CUDA_CHECK(test, !past_end_resource.ok());

  if (*count == 0) {
    return asc_core_cuda_test::kSkipReturnCode;
  }
  auto resource = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto context = asc::CreateCudaExecutionContext(0);
  ASC_CORE_CUDA_CHECK(test, resource.ok());
  ASC_CORE_CUDA_CHECK(test, context.ok());
  if (!resource.ok() || !context.ok()) {
    return test.Finish();
  }

  auto zero = (*resource)->Allocate(0, alignof(std::max_align_t));
  ASC_CORE_CUDA_CHECK(test, zero.ok());
  if (zero.ok()) {
    ASC_CORE_CUDA_EQ(test, *zero, nullptr);
  }

  auto zero_alignment = (*resource)->Allocate(64, 0);
  ASC_CORE_CUDA_CHECK(test, !zero_alignment.ok());
  auto non_power_of_two_alignment = (*resource)->Allocate(64, 3);
  ASC_CORE_CUDA_CHECK(test, !non_power_of_two_alignment.ok());
  auto excessive_alignment =
      (*resource)->Allocate(64, std::numeric_limits<std::size_t>::max());
  ASC_CORE_CUDA_CHECK(test, !excessive_alignment.ok());

  auto buffer = asc::Buffer::Allocate(**resource, 128, alignof(std::uint64_t));
  ASC_CORE_CUDA_CHECK(test, buffer.ok());
  if (!buffer.ok()) {
    return test.Finish();
  }
  auto* bytes = static_cast<std::byte*>(buffer->data());

  auto overlap = asc::CopyBytes(
      *context,
      asc::MutableMemoryView(bytes + 1, 64, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(bytes, 64, asc::MemorySpace::kDevice), 64);
  ASC_CORE_CUDA_CHECK(test, !overlap.ok());
  if (!overlap.ok()) {
    ASC_CORE_CUDA_EQ(test, overlap.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  }

  auto null_source = asc::CopyBytes(
      *context, asc::MutableMemoryView(bytes, 64, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(nullptr, 64, asc::MemorySpace::kDevice), 64);
  ASC_CORE_CUDA_CHECK(test, !null_source.ok());
  auto null_destination = asc::CopyBytes(
      *context, asc::MutableMemoryView(nullptr, 64, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(bytes, 64, asc::MemorySpace::kDevice), 64);
  ASC_CORE_CUDA_CHECK(test, !null_destination.ok());

  std::array<std::byte, 16> ordinary_host_storage{};
  auto mislabeled_self_copy =
      asc::CopyBytes(*context,
                     asc::MutableMemoryView(ordinary_host_storage.data(),
                                            ordinary_host_storage.size(),
                                            asc::MemorySpace::kDevice),
                     asc::ConstMemoryView(ordinary_host_storage.data(),
                                          ordinary_host_storage.size(),
                                          asc::MemorySpace::kDevice));
  ASC_CORE_CUDA_CHECK(test, !mislabeled_self_copy.ok());
  if (!mislabeled_self_copy.ok()) {
    ASC_CORE_CUDA_EQ(test, mislabeled_self_copy.status().code(),
                     asc::ErrorCode::kMemoryAccess);
  }

  auto too_large = asc::CopyBytes(
      *context, asc::MutableMemoryView(bytes, 64, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(bytes + 64, 64, asc::MemorySpace::kDevice), 65);
  ASC_CORE_CUDA_CHECK(test, !too_large.ok());

  auto serial_event = asc::RecordCudaEvent(asc::ExecutionContext::Serial());
  ASC_CORE_CUDA_CHECK(test, !serial_event.ok());
  if (!serial_event.ok()) {
    ASC_CORE_CUDA_EQ(test, serial_event.status().code(),
                     asc::ErrorCode::kInvalidArgument);
  }

  return test.Finish();
}
