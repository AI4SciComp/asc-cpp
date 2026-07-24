#include <asc/core/memory_resource.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "test_resources.h"

namespace asc {
namespace {

TEST(HostMemoryResourceTest, ReturnsStableHostResource) {
  const MemoryResourcePtr first = GetHostMemoryResource();
  const MemoryResourcePtr second = GetHostMemoryResource();

  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first, second);
  EXPECT_EQ(first->GetMemorySpace(), MemorySpace::kHost);
  EXPECT_EQ(first->GetName(), "host");
  EXPECT_TRUE(first->IsEqual(*second));
}

TEST(HostMemoryResourceTest, ZeroByteAllocationReturnsNull) {
  const MemoryResourcePtr resource = GetHostMemoryResource();
  Result<void*> allocation = resource->Allocate(0, alignof(std::max_align_t));

  ASSERT_TRUE(allocation.ok());
  EXPECT_EQ(allocation.value(), nullptr);
  resource->Deallocate(nullptr, 0, alignof(std::max_align_t));
}

TEST(HostMemoryResourceTest, RejectsInvalidAlignment) {
  const MemoryResourcePtr resource = GetHostMemoryResource();

  Result<void*> zero_alignment = resource->Allocate(8, 0);
  ASSERT_FALSE(zero_alignment.ok());
  EXPECT_EQ(zero_alignment.status().code(), StatusCode::kInvalidArgument);

  Result<void*> non_power_of_two = resource->Allocate(8, 3);
  ASSERT_FALSE(non_power_of_two.ok());
  EXPECT_EQ(non_power_of_two.status().code(),
            StatusCode::kInvalidArgument);
}

TEST(HostMemoryResourceTest, HonorsOverAlignment) {
  constexpr std::size_t kBytes = 256;
  constexpr std::size_t kAlignment = 64;
  const MemoryResourcePtr resource = GetHostMemoryResource();
  Result<void*> allocation = resource->Allocate(kBytes, kAlignment);

  ASSERT_TRUE(allocation.ok());
  ASSERT_NE(allocation.value(), nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(allocation.value()) % kAlignment,
            0U);
  resource->Deallocate(allocation.value(), kBytes, kAlignment);
}

TEST(MemoryResourceTest, CountingResourceReportsMatchingOperations) {
  auto resource = std::make_shared<test::CountingMemoryResource>();
  Result<void*> allocation = resource->Allocate(32, 16);
  ASSERT_TRUE(allocation.ok());

  resource->Deallocate(allocation.value(), 32, 16);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 1);
  EXPECT_EQ(resource->allocated_bytes.load(), 32U);
  EXPECT_EQ(resource->deallocated_bytes.load(), 32U);
  EXPECT_EQ(resource->last_alignment.load(), 16U);
}

TEST(MemoryResourceTest, FailingResourceIsDeterministic) {
  auto resource = std::make_shared<test::FailingMemoryResource>();
  Result<void*> allocation = resource->Allocate(128, alignof(double));

  ASSERT_FALSE(allocation.ok());
  EXPECT_EQ(allocation.status().code(), StatusCode::kAllocationFailed);
  EXPECT_EQ(resource->allocation_calls.load(), 1);
  EXPECT_EQ(resource->deallocation_calls.load(), 0);
}

TEST(HostMemoryResourceTest, SupportsConcurrentAllocation) {
  constexpr int kThreadCount = 4;
  constexpr int kIterations = 100;
  const MemoryResourcePtr resource = GetHostMemoryResource();
  std::atomic<bool> all_allocations_succeeded = true;
  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);

  for (int thread = 0; thread < kThreadCount; ++thread) {
    threads.emplace_back([&all_allocations_succeeded, resource] {
      for (int iteration = 0; iteration < kIterations; ++iteration) {
        Result<void*> allocation = resource->Allocate(32, 16);
        if (!allocation.ok()) {
          all_allocations_succeeded.store(false, std::memory_order_relaxed);
          return;
        }
        resource->Deallocate(allocation.value(), 32, 16);
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT_TRUE(all_allocations_succeeded.load(std::memory_order_relaxed));
}

}  // namespace
}  // namespace asc
