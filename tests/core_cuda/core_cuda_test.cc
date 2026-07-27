#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "test_support.h"

namespace {

static_assert(!std::is_copy_constructible_v<asc::CudaMemoryResource>);
static_assert(!std::is_copy_assignable_v<asc::CudaMemoryResource>);
static_assert(!std::is_move_constructible_v<asc::CudaMemoryResource>);
static_assert(!std::is_move_assignable_v<asc::CudaMemoryResource>);
static_assert(std::is_copy_constructible_v<asc::ExecutionContext>);
static_assert(std::is_copy_assignable_v<asc::ExecutionContext>);
static_assert(!std::is_copy_constructible_v<asc::CompletionEvent>);
static_assert(!std::is_copy_assignable_v<asc::CompletionEvent>);
static_assert(std::is_nothrow_move_constructible_v<asc::CompletionEvent>);
static_assert(std::is_nothrow_move_assignable_v<asc::CompletionEvent>);

asc::Device CudaDevice(std::int32_t ordinal = 0) {
  return asc::Device{asc::Backend::kCuda, ordinal};
}

asc::Result<asc::CompletionEvent> MakeEventWhoseContextValueIsDestroyed() {
  auto context = asc::CreateCudaExecutionContext(CudaDevice());
  if (!context.ok()) {
    return context.status();
  }
  return asc::RecordCudaEvent(*context);
}

void FillPattern(asc::Buffer& buffer, std::uint8_t salt,
                 asc_core_cuda_test::TestContext& test) {
  auto view = buffer.mutable_view();
  ASC_CORE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return;
  }
  auto* bytes = static_cast<std::byte*>(view->data());
  for (std::size_t index = 0; index < view->size(); ++index) {
    bytes[index] = static_cast<std::byte>(
        (index * 37U + static_cast<std::size_t>(salt)) & 0xffU);
  }
}

void CheckEqualBytes(const asc::Buffer& left, const asc::Buffer& right,
                     asc_core_cuda_test::TestContext& test) {
  ASC_CORE_CUDA_EQ(test, left.size(), right.size());
  if (left.size() != right.size()) {
    return;
  }
  ASC_CORE_CUDA_CHECK(test,
                      std::memcmp(left.data(), right.data(), left.size()) == 0);
}

void CheckDeviceInventory(asc_core_cuda_test::TestContext& test) {
  const auto count = asc::CudaDeviceCount();
  ASC_CORE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return;
  }
  ASC_CORE_CUDA_CHECK(test, *count > 0);

  const auto invalid_context =
      asc::CreateCudaExecutionContext(CudaDevice(*count));
  ASC_CORE_CUDA_CHECK(test, !invalid_context.ok());
  if (!invalid_context.ok()) {
    ASC_CORE_CUDA_CHECK(
        test, invalid_context.status().code() == asc::ErrorCode::kUnavailable ||
                  invalid_context.status().code() ==
                      asc::ErrorCode::kInvalidArgument ||
                  invalid_context.status().code() == asc::ErrorCode::kProvider);
  }

  const auto invalid_resource = asc::CudaMemoryResource::Create(
      CudaDevice(*count), asc::MemorySpace::kDevice);
  ASC_CORE_CUDA_CHECK(test, !invalid_resource.ok());

  const auto host_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kHost);
  ASC_CORE_CUDA_CHECK(test, !host_resource.ok());

  const auto invalid_space = asc::CudaMemoryResource::Create(
      CudaDevice(), static_cast<asc::MemorySpace>(255));
  ASC_CORE_CUDA_CHECK(test, !invalid_space.ok());
}

void CheckResources(asc_core_cuda_test::TestContext& test) {
  for (const asc::MemorySpace space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    auto resource = asc::CudaMemoryResource::Create(CudaDevice(), space);
    ASC_CORE_CUDA_CHECK(test, resource.ok());
    if (!resource.ok()) {
      continue;
    }
    ASC_CORE_CUDA_EQ(test, (*resource)->space(), space);

    auto zero = asc::Buffer::Allocate(**resource, 0, 64);
    ASC_CORE_CUDA_CHECK(test, zero.ok());
    if (zero.ok()) {
      ASC_CORE_CUDA_CHECK(test, zero->valid());
      ASC_CORE_CUDA_EQ(test, zero->data(), nullptr);
      ASC_CORE_CUDA_EQ(test, zero->size(), std::size_t{0});
      zero->Reset();
      zero->Reset();
    }

    auto allocation = asc::Buffer::Allocate(**resource, 513, 64);
    ASC_CORE_CUDA_CHECK(test, allocation.ok());
    if (allocation.ok()) {
      ASC_CORE_CUDA_CHECK(test, allocation->data() != nullptr);
      ASC_CORE_CUDA_EQ(
          test, reinterpret_cast<std::uintptr_t>(allocation->data()) % 64U,
          std::uintptr_t{0});
      void* pointer = allocation->data();
      asc::Buffer moved(std::move(*allocation));
      ASC_CORE_CUDA_CHECK(test, !allocation->valid());
      ASC_CORE_CUDA_EQ(test, moved.data(), pointer);
      moved.Reset();
      moved.Reset();
    }

    const auto bad_alignment = asc::Buffer::Allocate(**resource, 16, 3);
    ASC_CORE_CUDA_CHECK(test, !bad_alignment.ok());
  }

  auto device_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  ASC_CORE_CUDA_CHECK(test, device_resource.ok());
  if (!device_resource.ok()) {
    return;
  }
  const auto impossible = asc::Buffer::Allocate(
      **device_resource, std::numeric_limits<std::size_t>::max() / 2U, 64);
  ASC_CORE_CUDA_CHECK(test, !impossible.ok());
  if (!impossible.ok()) {
    ASC_CORE_CUDA_CHECK(
        test, impossible.status().code() == asc::ErrorCode::kAllocation ||
                  impossible.status().code() == asc::ErrorCode::kProvider ||
                  impossible.status().code() == asc::ErrorCode::kOverflow);
    if (impossible.status().code() == asc::ErrorCode::kAllocation ||
        impossible.status().code() == asc::ErrorCode::kProvider) {
      ASC_CORE_CUDA_CHECK(test, !impossible.status().provider().empty());
      ASC_CORE_CUDA_CHECK(test, impossible.status().native_code() != 0);
    }
  }
}

void CheckCopyAndEvents(asc_core_cuda_test::TestContext& test) {
  auto cuda_context = asc::CreateCudaExecutionContext(CudaDevice());
  ASC_CORE_CUDA_CHECK(test, cuda_context.ok());
  if (!cuda_context.ok()) {
    return;
  }
  ASC_CORE_CUDA_EQ(test, cuda_context->backend(), asc::Backend::kCuda);
  ASC_CORE_CUDA_EQ(test, cuda_context->device(), CudaDevice());
  ASC_CORE_CUDA_CHECK(test,
                      cuda_context->CanAccess(asc::MemorySpace::kPinnedHost));
  ASC_CORE_CUDA_CHECK(test, cuda_context->CanAccess(asc::MemorySpace::kDevice));
  ASC_CORE_CUDA_CHECK(test,
                      cuda_context->CanAccess(asc::MemorySpace::kManaged));

  const asc::ExecutionContext copied_context = *cuda_context;
  ASC_CORE_CUDA_EQ(test, copied_context.backend(), asc::Backend::kCuda);
  ASC_CORE_CUDA_EQ(test, copied_context.device(), CudaDevice());

  asc::HostMemoryResource host_resource;
  auto pinned_resource = asc::CudaMemoryResource::Create(
      CudaDevice(), asc::MemorySpace::kPinnedHost);
  auto device_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kDevice);
  auto managed_resource =
      asc::CudaMemoryResource::Create(CudaDevice(), asc::MemorySpace::kManaged);
  ASC_CORE_CUDA_CHECK(test, pinned_resource.ok());
  ASC_CORE_CUDA_CHECK(test, device_resource.ok());
  ASC_CORE_CUDA_CHECK(test, managed_resource.ok());
  if (!pinned_resource.ok() || !device_resource.ok() ||
      !managed_resource.ok()) {
    return;
  }

  constexpr std::size_t kBytes = 4096;
  auto host_source = asc::Buffer::Allocate(host_resource, kBytes, 64);
  auto host_result = asc::Buffer::Allocate(host_resource, kBytes, 64);
  auto pinned = asc::Buffer::Allocate(**pinned_resource, kBytes, 64);
  auto device = asc::Buffer::Allocate(**device_resource, kBytes, 64);
  auto second_device = asc::Buffer::Allocate(**device_resource, kBytes, 64);
  auto managed = asc::Buffer::Allocate(**managed_resource, kBytes, 64);
  ASC_CORE_CUDA_CHECK(test, host_source.ok());
  ASC_CORE_CUDA_CHECK(test, host_result.ok());
  ASC_CORE_CUDA_CHECK(test, pinned.ok());
  ASC_CORE_CUDA_CHECK(test, device.ok());
  ASC_CORE_CUDA_CHECK(test, second_device.ok());
  ASC_CORE_CUDA_CHECK(test, managed.ok());
  if (!host_source.ok() || !host_result.ok() || !pinned.ok() || !device.ok() ||
      !second_device.ok() || !managed.ok()) {
    return;
  }
  FillPattern(*host_source, 19, test);
  std::memset(host_result->data(), 0, host_result->size());

  const auto CopyAndWait = [&](asc::MutableMemoryView destination,
                               asc::ConstMemoryView source) {
    auto event =
        asc::CopyBytes(*cuda_context, destination, source, source.size());
    ASC_CORE_CUDA_CHECK(test, event.ok());
    if (!event.ok()) {
      return false;
    }
    auto queried = event->Query();
    ASC_CORE_CUDA_CHECK(test, queried.ok());
    const asc::Status waited = event->Wait();
    ASC_CORE_CUDA_CHECK(test, waited.ok());
    auto completed = event->Query();
    ASC_CORE_CUDA_CHECK(test, completed.ok());
    if (completed.ok()) {
      ASC_CORE_CUDA_CHECK(test, *completed);
    }
    return waited.ok();
  };

  ASC_CORE_CUDA_CHECK(
      test, CopyAndWait(*device->mutable_view(), *host_source->const_view()));
  ASC_CORE_CUDA_CHECK(
      test, CopyAndWait(*second_device->mutable_view(), *device->const_view()));
  ASC_CORE_CUDA_CHECK(
      test, CopyAndWait(*pinned->mutable_view(), *second_device->const_view()));
  ASC_CORE_CUDA_CHECK(
      test, CopyAndWait(*managed->mutable_view(), *device->const_view()));
  ASC_CORE_CUDA_CHECK(
      test, CopyAndWait(*host_result->mutable_view(), *managed->const_view()));
  CheckEqualBytes(*host_source, *host_result, test);
  ASC_CORE_CUDA_CHECK(
      test, std::memcmp(host_source->data(), pinned->data(), kBytes) == 0);
  ASC_CORE_CUDA_CHECK(
      test, CopyAndWait(*device->mutable_view(), *pinned->const_view()));
  ASC_CORE_CUDA_CHECK(test, CopyAndWait(*second_device->mutable_view(),
                                        *managed->const_view()));

  std::vector<std::byte> replacement(kBytes, std::byte{0xa5});
  for (asc::Buffer* cuda_allocation : {&*pinned, &*device, &*managed}) {
    ASC_CORE_CUDA_CHECK(
        test,
        CopyAndWait(*cuda_allocation->mutable_view(),
                    asc::ConstMemoryView(replacement.data(), replacement.size(),
                                         asc::MemorySpace::kHost)));
    ASC_CORE_CUDA_CHECK(test, CopyAndWait(*second_device->mutable_view(),
                                          *host_source->const_view()));

    const auto mislabeled_source = asc::CopyBytes(
        *cuda_context, *second_device->mutable_view(),
        asc::ConstMemoryView(cuda_allocation->data(), cuda_allocation->size(),
                             asc::MemorySpace::kHost),
        kBytes);
    ASC_CORE_CUDA_CHECK(test, !mislabeled_source.ok());
    std::memset(host_result->data(), 0, host_result->size());
    ASC_CORE_CUDA_CHECK(test, CopyAndWait(*host_result->mutable_view(),
                                          *second_device->const_view()));
    CheckEqualBytes(*host_source, *host_result, test);

    ASC_CORE_CUDA_CHECK(test, CopyAndWait(*cuda_allocation->mutable_view(),
                                          *host_source->const_view()));
    const auto mislabeled_destination = asc::CopyBytes(
        *cuda_context,
        asc::MutableMemoryView(cuda_allocation->data(), cuda_allocation->size(),
                               asc::MemorySpace::kHost),
        asc::ConstMemoryView(replacement.data(), replacement.size(),
                             asc::MemorySpace::kHost),
        kBytes);
    ASC_CORE_CUDA_CHECK(test, !mislabeled_destination.ok());
    std::memset(host_result->data(), 0, host_result->size());
    ASC_CORE_CUDA_CHECK(test, CopyAndWait(*host_result->mutable_view(),
                                          *cuda_allocation->const_view()));
    CheckEqualBytes(*host_source, *host_result, test);
  }

  auto exact_self = asc::CopyBytes(*cuda_context, *device->mutable_view(),
                                   *device->const_view(), kBytes);
  ASC_CORE_CUDA_CHECK(test, exact_self.ok());
  if (exact_self.ok()) {
    ASC_CORE_CUDA_CHECK(test, exact_self->Wait().ok());
  }

  auto* device_bytes = static_cast<std::byte*>(device->data());
  const asc::MutableMemoryView overlapping_destination(
      device_bytes + 1, kBytes - 1, asc::MemorySpace::kDevice);
  const asc::ConstMemoryView overlapping_source(device_bytes, kBytes - 1,
                                                asc::MemorySpace::kDevice);
  const auto overlap = asc::CopyBytes(*cuda_context, overlapping_destination,
                                      overlapping_source, kBytes - 1);
  ASC_CORE_CUDA_CHECK(test, !overlap.ok());

  const auto too_small = asc::CopyBytes(
      *cuda_context,
      asc::MutableMemoryView(device->data(), 8, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(host_source->data(), 16, asc::MemorySpace::kHost),
      16);
  ASC_CORE_CUDA_CHECK(test, !too_small.ok());

  const auto null_nonempty = asc::CopyBytes(
      *cuda_context,
      asc::MutableMemoryView(nullptr, 1, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(host_source->data(), 1, asc::MemorySpace::kHost), 1);
  ASC_CORE_CUDA_CHECK(test, !null_nonempty.ok());

  const auto wrong_context =
      asc::CopyBytes(asc::ExecutionContext::Serial(), *device->mutable_view(),
                     *host_source->const_view(), kBytes);
  ASC_CORE_CUDA_CHECK(test, !wrong_context.ok());

  const std::uintptr_t device_address =
      reinterpret_cast<std::uintptr_t>(device->data());
  const std::uintptr_t second_device_address =
      reinterpret_cast<std::uintptr_t>(second_device->data());
  ASC_CORE_CUDA_CHECK(test, device_address != second_device_address);
  if (device_address != second_device_address) {
    void* lower_pointer = device_address < second_device_address
                              ? device->data()
                              : second_device->data();
    void* higher_pointer = device_address < second_device_address
                               ? second_device->data()
                               : device->data();
    const std::uintptr_t higher_address =
        reinterpret_cast<std::uintptr_t>(higher_pointer);
    const std::uintptr_t overflowing_size =
        std::numeric_limits<std::uintptr_t>::max() - higher_address + 1U;
    if constexpr (sizeof(std::size_t) >= sizeof(std::uintptr_t)) {
      const std::size_t byte_count = static_cast<std::size_t>(overflowing_size);
      const auto source_end_overflow =
          asc::CopyBytes(*cuda_context,
                         asc::MutableMemoryView(lower_pointer, byte_count,
                                                asc::MemorySpace::kDevice),
                         asc::ConstMemoryView(higher_pointer, byte_count,
                                              asc::MemorySpace::kDevice),
                         byte_count);
      ASC_CORE_CUDA_CHECK(test, !source_end_overflow.ok());
      if (!source_end_overflow.ok()) {
        ASC_CORE_CUDA_EQ(test, source_end_overflow.status().code(),
                         asc::ErrorCode::kOverflow);
      }

      const auto destination_end_overflow =
          asc::CopyBytes(*cuda_context,
                         asc::MutableMemoryView(higher_pointer, byte_count,
                                                asc::MemorySpace::kDevice),
                         asc::ConstMemoryView(lower_pointer, byte_count,
                                              asc::MemorySpace::kDevice),
                         byte_count);
      ASC_CORE_CUDA_CHECK(test, !destination_end_overflow.ok());
      if (!destination_end_overflow.ok()) {
        ASC_CORE_CUDA_EQ(test, destination_end_overflow.status().code(),
                         asc::ErrorCode::kOverflow);
      }
    }
  }

  std::uint32_t mislabeled_host_storage = 0x12345678U;
  const auto invalid_exact_self =
      asc::CopyBytes(*cuda_context,
                     asc::MutableMemoryView(&mislabeled_host_storage,
                                            sizeof(mislabeled_host_storage),
                                            asc::MemorySpace::kDevice),
                     asc::ConstMemoryView(&mislabeled_host_storage,
                                          sizeof(mislabeled_host_storage),
                                          asc::MemorySpace::kDevice),
                     sizeof(mislabeled_host_storage));
  ASC_CORE_CUDA_CHECK(test, !invalid_exact_self.ok());
  ASC_CORE_CUDA_EQ(test, mislabeled_host_storage, std::uint32_t{0x12345678U});

  auto event = asc::RecordCudaEvent(*cuda_context);
  ASC_CORE_CUDA_CHECK(test, event.ok());
  if (event.ok()) {
    asc::CompletionEvent moved(std::move(*event));
    ASC_CORE_CUDA_CHECK(test, !event->valid());
    ASC_CORE_CUDA_CHECK(test, moved.valid());
    ASC_CORE_CUDA_CHECK(test, !event->Query().ok());
    ASC_CORE_CUDA_CHECK(test, !event->Wait().ok());
    ASC_CORE_CUDA_CHECK(test, moved.Wait().ok());
    auto query = moved.Query();
    ASC_CORE_CUDA_CHECK(test, query.ok());
    if (query.ok()) {
      ASC_CORE_CUDA_CHECK(test, *query);
    }
  }

  auto retained_event = MakeEventWhoseContextValueIsDestroyed();
  ASC_CORE_CUDA_CHECK(test, retained_event.ok());
  if (retained_event.ok()) {
    ASC_CORE_CUDA_CHECK(test, retained_event->Wait().ok());
  }

  auto zero = asc::CopyBytes(
      *cuda_context,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice),
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost), 0);
  ASC_CORE_CUDA_CHECK(test, zero.ok());
  if (zero.ok()) {
    ASC_CORE_CUDA_CHECK(test, zero->Wait().ok());
  }
}

void CheckProviderFreeCreation(asc_core_cuda_test::TestContext& test) {
  const auto unavailable = asc::ExecutionContext::Create(
      asc::Backend::kCuda, CudaDevice(), asc::Determinism::kDeterministic);
  ASC_CORE_CUDA_CHECK(test, !unavailable.ok());
  if (!unavailable.ok()) {
    ASC_CORE_CUDA_EQ(test, unavailable.status().code(),
                     asc::ErrorCode::kUnavailable);
  }
}

}  // namespace

int main() {
  asc_core_cuda_test::TestContext test;
  CheckDeviceInventory(test);
  CheckResources(test);
  CheckCopyAndEvents(test);
  CheckProviderFreeCreation(test);
  return test.Finish();
}
