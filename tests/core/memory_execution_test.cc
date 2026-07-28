#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "test_support.h"

namespace {

static_assert(!std::is_copy_constructible_v<asc::Buffer>);
static_assert(!std::is_copy_assignable_v<asc::Buffer>);
static_assert(std::is_nothrow_move_constructible_v<asc::Buffer>);
static_assert(std::is_nothrow_move_assignable_v<asc::Buffer>);
static_assert(std::is_nothrow_destructible_v<asc::Buffer>);
static_assert(!std::is_copy_constructible_v<asc::CompletionEvent>);
static_assert(!std::is_copy_assignable_v<asc::CompletionEvent>);
static_assert(std::is_nothrow_move_constructible_v<asc::CompletionEvent>);
static_assert(std::is_nothrow_move_assignable_v<asc::CompletionEvent>);

class CountingResource final : public asc::MemoryResource {
 public:
  enum class Behavior {
    kDelegate,
    kFailure,
    kNull,
    kNonNullForZero,
  };

  explicit CountingResource(asc::MemorySpace space = asc::MemorySpace::kHost,
                            Behavior behavior = Behavior::kDelegate)
      : space_(space), behavior_(behavior) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return space_;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocate_calls_;
    if (behavior_ == Behavior::kFailure) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "injected allocation failure");
    }
    if (behavior_ == Behavior::kNull) {
      return static_cast<void*>(nullptr);
    }
    if (behavior_ == Behavior::kNonNullForZero && bytes == 0) {
      auto allocation = host_.Allocate(1, alignment);
      if (allocation.ok()) {
        zero_allocation_ = *allocation;
      }
      return allocation;
    }
    return host_.Allocate(bytes, alignment);
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocate_calls_;
    last_deallocated_pointer_ = pointer;
    if (pointer == zero_allocation_) {
      host_.Deallocate(pointer, 1, alignment);
      zero_allocation_ = nullptr;
    } else {
      host_.Deallocate(pointer, bytes, alignment);
    }
  }

  [[nodiscard]] int allocate_calls() const noexcept { return allocate_calls_; }
  [[nodiscard]] int deallocate_calls() const noexcept {
    return deallocate_calls_;
  }
  [[nodiscard]] void* last_deallocated_pointer() const noexcept {
    return last_deallocated_pointer_;
  }

 private:
  asc::MemorySpace space_;
  Behavior behavior_;
  asc::HostMemoryResource host_;
  void* zero_allocation_ = nullptr;
  void* last_deallocated_pointer_ = nullptr;
  int allocate_calls_ = 0;
  int deallocate_calls_ = 0;
};

class MisalignedResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }

  asc::Result<void*> Allocate(std::size_t, std::size_t) override {
    ++allocate_calls_;
    return static_cast<void*>(storage_.data() + 1);
  }

  void Deallocate(void* pointer, std::size_t, std::size_t) noexcept override {
    ++deallocate_calls_;
    deallocated_pointer_ = pointer;
  }

  [[nodiscard]] int allocate_calls() const noexcept { return allocate_calls_; }
  [[nodiscard]] int deallocate_calls() const noexcept {
    return deallocate_calls_;
  }
  [[nodiscard]] void* returned_pointer() noexcept {
    return storage_.data() + 1;
  }
  [[nodiscard]] void* deallocated_pointer() const noexcept {
    return deallocated_pointer_;
  }

 private:
  alignas(64) std::array<std::byte, 128> storage_{};
  void* deallocated_pointer_ = nullptr;
  int allocate_calls_ = 0;
  int deallocate_calls_ = 0;
};

void CheckHostResource(asc_core_test::TestContext& context) {
  asc::HostMemoryResource resource;
  ASC_TEST_EQ(context, resource.space(), asc::MemorySpace::kHost);
  for (const std::size_t alignment : {std::size_t{1}, alignof(std::max_align_t),
                                      std::size_t{64}, std::size_t{4096}}) {
    auto allocation = resource.Allocate(257, alignment);
    ASC_TEST_CHECK(context, allocation.ok());
    const auto address = reinterpret_cast<std::uintptr_t>(*allocation);
    ASC_TEST_EQ(context, address % alignment, std::uintptr_t{0});
    resource.Deallocate(*allocation, 257, alignment);
  }
  ASC_TEST_CHECK(context, !resource.Allocate(1, 0).ok());
  ASC_TEST_CHECK(context, !resource.Allocate(1, 3).ok());
  const auto zero = resource.Allocate(0, 64);
  ASC_TEST_CHECK(context, zero.ok());
  ASC_TEST_EQ(context, *zero, nullptr);
}

void CheckBufferLifetime(asc_core_test::TestContext& context) {
  CountingResource resource;
  auto buffer = asc::Buffer::Allocate(resource, 96, 64);
  ASC_TEST_CHECK(context, buffer.ok());
  ASC_TEST_CHECK(context, buffer->valid());
  ASC_TEST_CHECK(context, buffer->data() != nullptr);
  ASC_TEST_EQ(context, buffer->size(), std::size_t{96});
  ASC_TEST_EQ(context, buffer->alignment(), std::size_t{64});
  ASC_TEST_EQ(context, *buffer->space(), asc::MemorySpace::kHost);
  ASC_TEST_CHECK(context, buffer->mutable_view()->valid());
  ASC_TEST_CHECK(context, buffer->const_view()->valid());
  ASC_TEST_EQ(context, resource.allocate_calls(), 1);

  void* allocation = buffer->data();
  asc::Buffer moved(std::move(*buffer));
  ASC_TEST_EQ(context, moved.data(), allocation);
  ASC_TEST_CHECK(context, !buffer->valid());
  ASC_TEST_CHECK(context, !buffer->space().ok());
  ASC_TEST_CHECK(context, !buffer->mutable_view().ok());
  ASC_TEST_CHECK(context, !buffer->const_view().ok());
  moved.Reset();
  moved.Reset();
  ASC_TEST_EQ(context, resource.deallocate_calls(), 1);
  ASC_TEST_EQ(context, resource.last_deallocated_pointer(), allocation);

  auto first = asc::Buffer::Allocate(resource, 32, 16);
  auto second = asc::Buffer::Allocate(resource, 48, 16);
  *first = std::move(*second);
  ASC_TEST_EQ(context, resource.deallocate_calls(), 2);
  ASC_TEST_CHECK(context, !second->valid());
  first->Reset();
  ASC_TEST_EQ(context, resource.deallocate_calls(), 3);
}

void CheckBufferFailures(asc_core_test::TestContext& context) {
  CountingResource zero_resource;
  auto zero = asc::Buffer::Allocate(zero_resource, 0, 32);
  ASC_TEST_CHECK(context, zero.ok());
  ASC_TEST_CHECK(context, zero->valid());
  ASC_TEST_EQ(context, zero->data(), nullptr);
  ASC_TEST_EQ(context, zero_resource.allocate_calls(), 1);
  zero->Reset();
  ASC_TEST_EQ(context, zero_resource.deallocate_calls(), 0);

  CountingResource failing(asc::MemorySpace::kHost,
                           CountingResource::Behavior::kFailure);
  ASC_TEST_EQ(context, asc::Buffer::Allocate(failing, 64, 16).status().code(),
              asc::ErrorCode::kAllocation);
  ASC_TEST_EQ(context, failing.deallocate_calls(), 0);

  CountingResource null_resource(asc::MemorySpace::kHost,
                                 CountingResource::Behavior::kNull);
  ASC_TEST_CHECK(context, !asc::Buffer::Allocate(null_resource, 64, 16).ok());
  ASC_TEST_EQ(context, null_resource.deallocate_calls(), 0);

  CountingResource bad_zero(asc::MemorySpace::kHost,
                            CountingResource::Behavior::kNonNullForZero);
  ASC_TEST_CHECK(context, !asc::Buffer::Allocate(bad_zero, 0, 16).ok());
  ASC_TEST_EQ(context, bad_zero.deallocate_calls(), 1);

  MisalignedResource misaligned;
  ASC_TEST_CHECK(context, !asc::Buffer::Allocate(misaligned, 32, 64).ok());
  ASC_TEST_EQ(context, misaligned.allocate_calls(), 1);
  ASC_TEST_EQ(context, misaligned.deallocate_calls(), 1);
  ASC_TEST_EQ(context, misaligned.deallocated_pointer(),
              misaligned.returned_pointer());
}

void CheckContexts(asc_core_test::TestContext& context) {
  const asc::ExecutionContext first = asc::ExecutionContext::Serial();
  const asc::ExecutionContext second = asc::ExecutionContext::Serial();
  ASC_TEST_EQ(context, first.backend(), asc::Backend::kSerial);
  ASC_TEST_EQ(context, first.device(), asc::Device::Serial());
  ASC_TEST_EQ(context, first.determinism(), asc::Determinism::kDeterministic);
  ASC_TEST_CHECK(context, first.CanAccess(asc::MemorySpace::kHost));
  ASC_TEST_CHECK(context, !first.CanAccess(asc::MemorySpace::kPinnedHost));
  ASC_TEST_CHECK(context, !first.CanAccess(asc::MemorySpace::kDevice));
  ASC_TEST_CHECK(context, !first.CanAccess(asc::MemorySpace::kManaged));
  ASC_TEST_EQ(context, second.backend(), asc::Backend::kSerial);

  const auto serial_default = asc::ExecutionContext::Create(
      asc::Backend::kSerial, asc::Device::Serial(),
      asc::Determinism::kBackendDefault);
  ASC_TEST_EQ(context, serial_default->determinism(),
              asc::Determinism::kBackendDefault);

  const auto mismatch = asc::ExecutionContext::Create(
      asc::Backend::kSerial, asc::Device{asc::Backend::kCuda, 0});
  ASC_TEST_EQ(context, mismatch.status().code(),
              asc::ErrorCode::kInvalidArgument);
  const auto negative = asc::ExecutionContext::Create(
      asc::Backend::kCuda, asc::Device{asc::Backend::kCuda, -1});
  ASC_TEST_EQ(context, negative.status().code(),
              asc::ErrorCode::kInvalidArgument);
  const auto serial_one = asc::ExecutionContext::Create(
      asc::Backend::kSerial, asc::Device{asc::Backend::kSerial, 1});
  ASC_TEST_EQ(context, serial_one.status().code(),
              asc::ErrorCode::kUnavailable);
  const auto cuda = asc::ExecutionContext::Create(
      asc::Backend::kCuda, asc::Device{asc::Backend::kCuda, 0});
  ASC_TEST_EQ(context, cuda.status().code(), asc::ErrorCode::kUnavailable);
  const auto hip = asc::ExecutionContext::Create(
      asc::Backend::kHip, asc::Device{asc::Backend::kHip, 0});
  ASC_TEST_EQ(context, hip.status().code(), asc::ErrorCode::kUnsupported);
  const auto sycl = asc::ExecutionContext::Create(
      asc::Backend::kSycl, asc::Device{asc::Backend::kSycl, 0});
  ASC_TEST_EQ(context, sycl.status().code(), asc::ErrorCode::kUnsupported);

  const auto invalid_backend = static_cast<asc::Backend>(255);
  ASC_TEST_EQ(context,
              asc::ExecutionContext::Create(invalid_backend,
                                            asc::Device{invalid_backend, 0})
                  .status()
                  .code(),
              asc::ErrorCode::kInvalidArgument);
  ASC_TEST_EQ(context,
              asc::ExecutionContext::Create(asc::Backend::kSerial,
                                            asc::Device::Serial(),
                                            static_cast<asc::Determinism>(255))
                  .status()
                  .code(),
              asc::ErrorCode::kInvalidArgument);
}

void CheckCopiesAndEvents(asc_core_test::TestContext& context) {
  const asc::ExecutionContext serial = asc::ExecutionContext::Serial();
  std::array<char, 6> storage = {'a', 'b', 'c', 'd', 'e', 'f'};
  auto copy = asc::CopyBytes(
      serial,
      asc::MutableMemoryView(storage.data() + 1, 4, asc::MemorySpace::kHost),
      asc::ConstMemoryView(storage.data(), 4, asc::MemorySpace::kHost));
  ASC_TEST_CHECK(context, copy.ok());
  ASC_TEST_CHECK(context, copy->valid());
  ASC_TEST_CHECK(context, copy->is_complete());
  ASC_TEST_EQ(context, *copy->Query(), true);
  ASC_TEST_CHECK(context, copy->Wait().ok());
  const std::array<char, 6> forward = {'a', 'a', 'b', 'c', 'd', 'f'};
  ASC_TEST_CHECK(context, storage == forward);

  storage = {'a', 'b', 'c', 'd', 'e', 'f'};
  auto backward = asc::CopyBytes(
      serial,
      asc::MutableMemoryView(storage.data(), 4, asc::MemorySpace::kHost),
      asc::ConstMemoryView(storage.data() + 1, 4, asc::MemorySpace::kHost));
  const std::array<char, 6> expected = {'b', 'c', 'd', 'e', 'e', 'f'};
  ASC_TEST_CHECK(context, storage == expected);

  asc::CompletionEvent moved(std::move(*copy));
  ASC_TEST_CHECK(context, moved.valid());
  ASC_TEST_CHECK(context, !copy->valid());
  ASC_TEST_EQ(context, copy->Wait().code(), asc::ErrorCode::kInvalidState);
  moved = std::move(*backward);
  ASC_TEST_CHECK(context, !backward->valid());

  auto zero_copy = asc::CopyBytes(
      serial, asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kHost),
      asc::ConstMemoryView(nullptr, 0, asc::MemorySpace::kHost), 0);
  ASC_TEST_CHECK(context, zero_copy.ok());

  ASC_TEST_EQ(
      context,
      asc::CopyBytes(
          serial,
          asc::MutableMemoryView(storage.data(), 2, asc::MemorySpace::kHost),
          asc::ConstMemoryView(storage.data(), 3, asc::MemorySpace::kHost), 3)
          .status()
          .code(),
      asc::ErrorCode::kMemoryAccess);
  ASC_TEST_EQ(
      context,
      asc::CopyBytes(
          serial,
          asc::MutableMemoryView(storage.data(), 1, asc::MemorySpace::kHost),
          asc::ConstMemoryView(nullptr, 1, asc::MemorySpace::kHost), 1)
          .status()
          .code(),
      asc::ErrorCode::kMemoryAccess);

  const auto before = storage;
  ASC_TEST_EQ(context,
              asc::CopyBytes(serial,
                             asc::MutableMemoryView(storage.data(), 1,
                                                    asc::MemorySpace::kHost),
                             asc::ConstMemoryView(storage.data() + 1, 1,
                                                  asc::MemorySpace::kDevice),
                             1)
                  .status()
                  .code(),
              asc::ErrorCode::kUnsupported);
  ASC_TEST_CHECK(context, storage == before);
  ASC_TEST_EQ(
      context,
      asc::CopyBytes(
          serial,
          asc::MutableMemoryView(storage.data(), 1, asc::MemorySpace::kManaged),
          asc::ConstMemoryView(storage.data() + 1, 1, asc::MemorySpace::kHost),
          1)
          .status()
          .code(),
      asc::ErrorCode::kUnsupported);
  ASC_TEST_CHECK(context, storage == before);
}

}  // namespace

int main() {
  asc_core_test::TestContext context;
  CheckHostResource(context);
  CheckBufferLifetime(context);
  CheckBufferFailures(context);
  CheckContexts(context);
  CheckCopiesAndEvents(context);
  return context.Finish();
}
