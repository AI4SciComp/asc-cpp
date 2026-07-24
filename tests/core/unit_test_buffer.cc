#include <asc/core/buffer.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include "test_resources.h"

namespace asc {
namespace {

struct TrackedValue {
  TrackedValue() { live.fetch_add(1, std::memory_order_relaxed); }
  ~TrackedValue() {
    live.fetch_sub(1, std::memory_order_relaxed);
    destroyed.fetch_add(1, std::memory_order_relaxed);
  }

  static inline std::atomic<int> live{0};
  static inline std::atomic<int> destroyed{0};
};

struct ThrowingValue {
  ThrowingValue() {
    const int attempt = attempts.fetch_add(1, std::memory_order_relaxed) + 1;
    if (attempt == fail_on_attempt.load(std::memory_order_relaxed)) {
      throw 17;
    }
    live.fetch_add(1, std::memory_order_relaxed);
  }

  ~ThrowingValue() { live.fetch_sub(1, std::memory_order_relaxed); }

  static inline std::atomic<int> attempts{0};
  static inline std::atomic<int> fail_on_attempt{0};
  static inline std::atomic<int> live{0};
};

struct NonDefaultValue {
  NonDefaultValue() = delete;
};

class NullMemoryResource final : public MemoryResource {
 public:
  MemorySpace GetMemorySpace() const noexcept override {
    return MemorySpace::kHost;
  }

  std::string_view GetName() const noexcept override { return "null"; }

  Result<void*> Allocate(std::size_t, std::size_t) override {
    ++allocation_calls;
    return static_cast<void*>(nullptr);
  }

  void Deallocate(void*, std::size_t, std::size_t) noexcept override {
    ++deallocation_calls;
  }

  bool IsEqual(const MemoryResource& other) const noexcept override {
    return this == &other;
  }

  std::atomic<int> allocation_calls{0};
  std::atomic<int> deallocation_calls{0};
};

static_assert(!std::is_copy_constructible_v<Buffer<int>>);
static_assert(!std::is_copy_assignable_v<Buffer<int>>);
static_assert(std::is_nothrow_move_constructible_v<Buffer<int>>);
static_assert(std::is_nothrow_move_assignable_v<Buffer<int>>);
static_assert(!std::is_convertible_v<Buffer<int>&, int*>);
static_assert(!std::is_convertible_v<const Buffer<int>&, const int*>);

TEST(BufferTest, DefaultBufferIsEmptyHostStorage) {
  Buffer<int> buffer;

  EXPECT_TRUE(buffer.IsEmpty());
  EXPECT_EQ(buffer.GetSize(), 0);
  EXPECT_EQ(buffer.GetByteSize(), 0U);
  EXPECT_EQ(buffer.GetMemorySpace(), MemorySpace::kHost);
  ASSERT_NE(buffer.GetMemoryResource(), nullptr);

  Result<int*> data = buffer.HostData();
  ASSERT_TRUE(data.ok());
  EXPECT_EQ(data.value(), nullptr);
}

TEST(BufferTest, ZeroSizeDoesNotCallResource) {
  auto resource = std::make_shared<test::CountingMemoryResource>();
  Result<Buffer<int>> result = Buffer<int>::Allocate(0, resource);

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.value().IsEmpty());
  EXPECT_EQ(result.value().GetMemoryResource(), resource);
  EXPECT_EQ(resource->allocation_calls.load(), 0);
  EXPECT_EQ(resource->deallocation_calls.load(), 0);
}

TEST(BufferTest, AllocationExposesCheckedMetadataAndHostData) {
  auto resource = std::make_shared<test::CountingMemoryResource>();
  Result<Buffer<int>> result = Buffer<int>::Allocate(4, resource);
  ASSERT_TRUE(result.ok());
  Buffer<int> buffer = std::move(result).value();

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(buffer.GetSize(), 4);
  EXPECT_EQ(buffer.GetByteSize(), 4U * sizeof(int));
  EXPECT_EQ(buffer.GetMemorySpace(), MemorySpace::kHost);
  EXPECT_EQ(buffer.GetMemoryResource(), resource);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->last_alignment.load(), alignof(int));

  Result<int*> mutable_data = buffer.HostData();
  ASSERT_TRUE(mutable_data.ok());
  for (int i = 0; i < 4; ++i) {
    mutable_data.value()[i] = i * i;
  }

  const Buffer<int>& const_buffer = buffer;
  Result<const int*> const_data = const_buffer.HostData();
  ASSERT_TRUE(const_data.ok());
  EXPECT_EQ(const_data.value()[3], 9);
}

TEST(BufferTest, RejectsNullNegativeAndOverflowBeforeResourceCall) {
  Result<Buffer<int>> null_resource = Buffer<int>::Allocate(1, nullptr);
  ASSERT_FALSE(null_resource.ok());
  EXPECT_EQ(null_resource.status().code(), StatusCode::kInvalidArgument);

  auto resource = std::make_shared<test::CountingMemoryResource>();
  Result<Buffer<int>> negative = Buffer<int>::Allocate(-1, resource);
  ASSERT_FALSE(negative.ok());
  EXPECT_EQ(negative.status().code(), StatusCode::kInvalidArgument);

  Result<Buffer<int>> overflow = Buffer<int>::Allocate(
      std::numeric_limits<extent_t>::max(), resource);
  ASSERT_FALSE(overflow.ok());
  EXPECT_EQ(overflow.status().code(), StatusCode::kOverflow);
  EXPECT_EQ(resource->allocation_calls.load(), 0);
}

TEST(BufferTest, PropagatesResourceFailureWithoutDeallocation) {
  auto resource = std::make_shared<test::FailingMemoryResource>();
  Result<Buffer<double>> result = Buffer<double>::Allocate(8, resource);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kAllocationFailed);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 0);
}

TEST(BufferTest, RejectsSuccessfulNullAllocation) {
  auto resource = std::make_shared<NullMemoryResource>();
  Result<Buffer<int>> result = Buffer<int>::Allocate(1, resource);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kAllocationFailed);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 0);
}

TEST(BufferTest, MoveConstructionTransfersExactlyOneOwnership) {
  auto resource = std::make_shared<test::CountingMemoryResource>();
  {
    Result<Buffer<int>> result = Buffer<int>::Allocate(3, resource);
    ASSERT_TRUE(result.ok());
    Buffer<int> source = std::move(result).value();
    Result<int*> source_data = source.HostData();
    ASSERT_TRUE(source_data.ok());
    int* original_pointer = source_data.value();

    Buffer<int> destination(std::move(source));
    EXPECT_TRUE(source.IsEmpty());
    EXPECT_EQ(source.GetSize(), 0);
    ASSERT_TRUE(destination.HostData().ok());
    EXPECT_EQ(destination.HostData().value(), original_pointer);
    EXPECT_EQ(resource->deallocation_calls.load(), 0);
  }
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 1);
  EXPECT_EQ(resource->allocated_bytes.load(),
            resource->deallocated_bytes.load());
}

TEST(BufferTest, MoveAssignmentReleasesPreviousAllocation) {
  auto resource = std::make_shared<test::CountingMemoryResource>();
  {
    Result<Buffer<int>> first_result = Buffer<int>::Allocate(2, resource);
    Result<Buffer<int>> second_result = Buffer<int>::Allocate(5, resource);
    ASSERT_TRUE(first_result.ok());
    ASSERT_TRUE(second_result.ok());
    Buffer<int> first = std::move(first_result).value();
    Buffer<int> second = std::move(second_result).value();

    first = std::move(second);
    EXPECT_EQ(first.GetSize(), 5);
    EXPECT_TRUE(second.IsEmpty());
    EXPECT_EQ(resource->deallocation_calls.load(), 1);

    first = std::move(first);
    EXPECT_EQ(first.GetSize(), 5);
  }
  EXPECT_EQ(resource->allocation_calls.load(), 2);
  EXPECT_EQ(resource->deallocation_calls.load(), 2);
  EXPECT_EQ(resource->allocated_bytes.load(),
            resource->deallocated_bytes.load());
}

TEST(BufferTest, ConstructsAndDestroysNonTrivialHostElements) {
  TrackedValue::live.store(0);
  TrackedValue::destroyed.store(0);
  {
    Result<Buffer<TrackedValue>> result =
        Buffer<TrackedValue>::Allocate(4);
    ASSERT_TRUE(result.ok());
    Buffer<TrackedValue> buffer = std::move(result).value();
    EXPECT_EQ(TrackedValue::live.load(), 4);
    EXPECT_EQ(TrackedValue::destroyed.load(), 0);
  }
  EXPECT_EQ(TrackedValue::live.load(), 0);
  EXPECT_EQ(TrackedValue::destroyed.load(), 4);
}

TEST(BufferTest, RollsBackPartiallyConstructedElements) {
  ThrowingValue::attempts.store(0);
  ThrowingValue::fail_on_attempt.store(3);
  ThrowingValue::live.store(0);
  auto resource = std::make_shared<test::CountingMemoryResource>();

  Result<Buffer<ThrowingValue>> result =
      Buffer<ThrowingValue>::Allocate(5, resource);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kAllocationFailed);
  EXPECT_EQ(ThrowingValue::attempts.load(), 3);
  EXPECT_EQ(ThrowingValue::live.load(), 0);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 1);
}

TEST(BufferTest, RejectsNonDefaultConstructibleHostElementsBeforeAllocation) {
  auto resource = std::make_shared<test::CountingMemoryResource>();
  Result<Buffer<NonDefaultValue>> result =
      Buffer<NonDefaultValue>::Allocate(1, resource);

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), StatusCode::kUnsupported);
  EXPECT_EQ(resource->allocation_calls.load(), 0);
}

TEST(BufferTest, DeviceSpaceDoesNotExposeHostPointer) {
  auto resource = std::make_shared<test::CountingMemoryResource>(
      GetHostMemoryResource(), MemorySpace::kDevice);
  Result<Buffer<int>> result = Buffer<int>::Allocate(2, resource);
  ASSERT_TRUE(result.ok());
  Buffer<int> buffer = std::move(result).value();

  Result<int*> host_data = buffer.HostData();
  ASSERT_FALSE(host_data.ok());
  EXPECT_EQ(host_data.status().code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
}

TEST(BufferTest, RetainsResourcePastCallersLocalHandle) {
  std::weak_ptr<test::CountingMemoryResource> weak_resource;
  Buffer<int> buffer;
  {
    auto resource = std::make_shared<test::CountingMemoryResource>();
    weak_resource = resource;
    Result<Buffer<int>> result = Buffer<int>::Allocate(2, resource);
    ASSERT_TRUE(result.ok());
    buffer = std::move(result).value();
  }

  EXPECT_FALSE(weak_resource.expired());
  buffer = Buffer<int>();
  EXPECT_TRUE(weak_resource.expired());
}

}  // namespace
}  // namespace asc
