#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "../../src/core/cuda/runtime_test_internal.h"  // NOLINT(misc-include-cleaner): CUDA fault-injection hooks.
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "test_support.h"

namespace {

using BytePattern = std::array<std::uint64_t, 257>;

void FillPattern(BytePattern& values) {
  for (std::size_t index = 0; index < values.size(); ++index) {
    values[index] =
        std::uint64_t{0x9e3779b97f4a7c15ULL} * (index + std::size_t{1});
  }
}

}  // namespace

// This end-to-end runtime test intentionally shares resources and injected
// fault state across the full lifecycle.
// NOLINTNEXTLINE(readability-function-size)
int main() {
  if (asc_core_cuda_test::ForceNoCudaDevice()) {
    return asc_core_cuda_test::kSkipReturnCode;
  }
  asc_core_cuda_test::TestContext test;

  auto device_count = asc::CudaDeviceCount();
  ASC_CORE_CUDA_CHECK(test, device_count.ok());
  if (!device_count.ok()) {
    return test.Finish();
  }
  if (*device_count == 0) {
    return asc_core_cuda_test::kSkipReturnCode;
  }

  auto context =
      asc::CreateCudaExecutionContext(0, asc::Determinism::kDeterministic);
  ASC_CORE_CUDA_CHECK(test, context.ok());
  if (!context.ok()) {
    return test.Finish();
  }
  ASC_CORE_CUDA_EQ(test, context->backend(), asc::Backend::kCuda);
  ASC_CORE_CUDA_EQ(test, context->device(),
                   (asc::Device{asc::Backend::kCuda, 0}));
  ASC_CORE_CUDA_EQ(test, context->determinism(),
                   asc::Determinism::kDeterministic);
  ASC_CORE_CUDA_CHECK(test, context->CanAccess(asc::MemorySpace::kPinnedHost));
  ASC_CORE_CUDA_CHECK(test, context->CanAccess(asc::MemorySpace::kDevice));
  ASC_CORE_CUDA_CHECK(test, context->CanAccess(asc::MemorySpace::kManaged));
  ASC_CORE_CUDA_CHECK(test, context->CanAccess(asc::MemorySpace::kHost));

  auto pinned_resource =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device_resource =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto managed_resource =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kManaged);
  ASC_CORE_CUDA_CHECK(test, pinned_resource.ok());
  ASC_CORE_CUDA_CHECK(test, device_resource.ok());
  ASC_CORE_CUDA_CHECK(test, managed_resource.ok());
  if (!pinned_resource.ok() || !device_resource.ok() ||
      !managed_resource.ok()) {
    return test.Finish();
  }

  auto source = asc::Buffer::Allocate(**pinned_resource, sizeof(BytePattern),
                                      alignof(std::uint64_t));
  auto device = asc::Buffer::Allocate(**device_resource, sizeof(BytePattern),
                                      alignof(std::uint64_t));
  auto destination = asc::Buffer::Allocate(
      **pinned_resource, sizeof(BytePattern), alignof(std::uint64_t));
  auto managed = asc::Buffer::Allocate(**managed_resource, sizeof(BytePattern),
                                       alignof(std::uint64_t));
  ASC_CORE_CUDA_CHECK(test, source.ok());
  ASC_CORE_CUDA_CHECK(test, device.ok());
  ASC_CORE_CUDA_CHECK(test, destination.ok());
  ASC_CORE_CUDA_CHECK(test, managed.ok());
  if (!source.ok() || !device.ok() || !destination.ok() || !managed.ok()) {
    return test.Finish();
  }

  auto* source_values = static_cast<BytePattern*>(source->data());
  auto* destination_values = static_cast<BytePattern*>(destination->data());
  FillPattern(*source_values);
  destination_values->fill(0);

  auto source_view = source->const_view();
  auto device_mutable_view = device->mutable_view();
  ASC_CORE_CUDA_CHECK(test, source_view.ok());
  ASC_CORE_CUDA_CHECK(test, device_mutable_view.ok());
#if defined(ASC_CPP_CUDA_RUNTIME_TEST_HOOKS)
  if (source_view.ok() && device_mutable_view.ok()) {
    const BytePattern expected = *source_values;
    asc::internal_core_cuda::ResetCudaRuntimeTestState();
    asc::internal_core_cuda::SetCudaRuntimeFault(
        asc::internal_core_cuda::CudaRuntimeFault::kEventRecord);
    auto record_failure =
        asc::CopyBytes(*context, *device_mutable_view, *source_view);
    ASC_CORE_CUDA_CHECK(test, !record_failure.ok());
    if (!record_failure.ok()) {
      ASC_CORE_CUDA_EQ(test, record_failure.status().code(),
                       asc::ErrorCode::kProvider);
      ASC_CORE_CUDA_EQ(test, record_failure.status().provider(), "cuda");
      ASC_CORE_CUDA_CHECK(test, record_failure.status().native_code() != 0);
    }
    ASC_CORE_CUDA_EQ(test, asc::internal_core_cuda::CudaRuntimeDrainCount(),
                     std::size_t{1});

    // Returning from the failed call must be a lifetime boundary: changing
    // the source immediately afterward cannot race the already-drained copy.
    source_values->fill(0);
    auto destination_view = destination->mutable_view();
    auto device_const_view = device->const_view();
    ASC_CORE_CUDA_CHECK(test, destination_view.ok());
    ASC_CORE_CUDA_CHECK(test, device_const_view.ok());
    if (destination_view.ok() && device_const_view.ok()) {
      auto recovered =
          asc::CopyBytes(*context, *destination_view, *device_const_view);
      ASC_CORE_CUDA_CHECK(test, recovered.ok());
      if (recovered.ok()) {
        ASC_CORE_CUDA_CHECK(test, recovered->Wait().ok());
      }
      ASC_CORE_CUDA_EQ(test, *destination_values, expected);
    }
    *source_values = expected;
    asc::internal_core_cuda::ResetCudaRuntimeTestState();
  }
#endif
  auto upload = asc::CopyBytes(*context, *device_mutable_view, *source_view);
  ASC_CORE_CUDA_CHECK(test, upload.ok());
  if (!upload.ok()) {
    return test.Finish();
  }
  ASC_CORE_CUDA_CHECK(test, upload->valid());
  auto upload_query = upload->Query();
  ASC_CORE_CUDA_CHECK(test, upload_query.ok());
  ASC_CORE_CUDA_CHECK(test, upload->Wait().ok());
  ASC_CORE_CUDA_CHECK(test, upload->is_complete());
  auto completed_query = upload->Query();
  ASC_CORE_CUDA_CHECK(test, completed_query.ok());
  if (completed_query.ok()) {
    ASC_CORE_CUDA_CHECK(test, *completed_query);
  }

  asc::CompletionEvent moved_upload = std::move(*upload);
  ASC_CORE_CUDA_CHECK(test, moved_upload.valid());
  ASC_CORE_CUDA_CHECK(test, moved_upload.Wait().ok());

  auto destination_view = destination->mutable_view();
  auto device_const_view = device->const_view();
  ASC_CORE_CUDA_CHECK(test, destination_view.ok());
  ASC_CORE_CUDA_CHECK(test, device_const_view.ok());
  auto download =
      asc::CopyBytes(*context, *destination_view, *device_const_view);
  ASC_CORE_CUDA_CHECK(test, download.ok());
  if (download.ok()) {
    ASC_CORE_CUDA_CHECK(test, download->Wait().ok());
  }
  ASC_CORE_CUDA_EQ(test, *destination_values, *source_values);

  destination_values->fill(0);
  auto managed_mutable_view = managed->mutable_view();
  auto managed_const_view = managed->const_view();
  ASC_CORE_CUDA_CHECK(test, managed_mutable_view.ok());
  ASC_CORE_CUDA_CHECK(test, managed_const_view.ok());
  if (managed_mutable_view.ok() && managed_const_view.ok()) {
    auto copy_to_managed =
        asc::CopyBytes(*context, *managed_mutable_view, *source_view);
    ASC_CORE_CUDA_CHECK(test, copy_to_managed.ok());
    if (copy_to_managed.ok()) {
      ASC_CORE_CUDA_CHECK(test, copy_to_managed->Wait().ok());
    }
    auto copy_from_managed =
        asc::CopyBytes(*context, *destination_view, *managed_const_view);
    ASC_CORE_CUDA_CHECK(test, copy_from_managed.ok());
    if (copy_from_managed.ok()) {
      ASC_CORE_CUDA_CHECK(test, copy_from_managed->Wait().ok());
    }
    ASC_CORE_CUDA_EQ(test, *destination_values, *source_values);
  }

  BytePattern pageable_source{};
  BytePattern pageable_destination{};
  FillPattern(pageable_source);
  pageable_destination.fill(0);
  auto pageable_upload = asc::CopyBytes(
      *context, *device_mutable_view,
      asc::ConstMemoryView(&pageable_source, sizeof(pageable_source),
                           asc::MemorySpace::kHost));
  ASC_CORE_CUDA_CHECK(test, pageable_upload.ok());
  if (pageable_upload.ok()) {
    ASC_CORE_CUDA_CHECK(test, pageable_upload->Wait().ok());
  }
  auto pageable_download =
      asc::CopyBytes(*context,
                     asc::MutableMemoryView(&pageable_destination,
                                            sizeof(pageable_destination),
                                            asc::MemorySpace::kHost),
                     *device_const_view);
  ASC_CORE_CUDA_CHECK(test, pageable_download.ok());
  if (pageable_download.ok()) {
    ASC_CORE_CUDA_CHECK(test, pageable_download->Wait().ok());
  }
  ASC_CORE_CUDA_EQ(test, pageable_destination, pageable_source);

  auto device_self_view = device->mutable_view();
  ASC_CORE_CUDA_CHECK(test, device_self_view.ok());
  if (device_self_view.ok()) {
    auto self_copy =
        asc::CopyBytes(*context, *device_self_view, *device_self_view);
    ASC_CORE_CUDA_CHECK(test, self_copy.ok());
    if (self_copy.ok()) {
      ASC_CORE_CUDA_CHECK(test, self_copy->Wait().ok());
      auto self_query = self_copy->Query();
      ASC_CORE_CUDA_CHECK(test, self_query.ok());
      if (self_query.ok()) {
        ASC_CORE_CUDA_CHECK(test, *self_query);
      }
    }
  }

  auto recorded = asc::RecordCudaEvent(*context);
  ASC_CORE_CUDA_CHECK(test, recorded.ok());
  if (recorded.ok()) {
    auto query = recorded->Query();
    ASC_CORE_CUDA_CHECK(test, query.ok());
    ASC_CORE_CUDA_CHECK(test, recorded->Wait().ok());
  }

  return test.Finish();
}
