#include <asc/core/execution_context.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>

namespace asc {
namespace {

static_assert(std::is_nothrow_copy_constructible_v<ExecutionContext>);
static_assert(std::is_nothrow_copy_assignable_v<ExecutionContext>);

TEST(ExecutionContextTest, SerialFactoryHasDeterministicDefaults) {
  const ExecutionContext context = ExecutionContext::Serial();

  EXPECT_EQ(context.GetBackend(), BackendKind::kSerial);
  EXPECT_EQ(context.GetDeviceId(), 0);
  EXPECT_EQ(context.GetFallbackPolicy(), FallbackPolicy::kDisallow);
  EXPECT_EQ(context.GetDeterminismPolicy(),
            DeterminismPolicy::kRequireDeterministic);
}

TEST(ExecutionContextTest, SerialCapabilitiesMatchImplementedProvider) {
  const ExecutionContext context = ExecutionContext::Serial();

  EXPECT_TRUE(context.Supports(ExecutionCapability::kSynchronous));
  EXPECT_FALSE(context.Supports(ExecutionCapability::kAsynchronous));
  EXPECT_TRUE(context.Supports(ExecutionCapability::kHostMemory));
  EXPECT_FALSE(context.Supports(ExecutionCapability::kPinnedHostMemory));
  EXPECT_FALSE(context.Supports(ExecutionCapability::kDeviceMemory));
  EXPECT_FALSE(context.Supports(ExecutionCapability::kManagedMemory));
}

TEST(ExecutionContextTest, CreatePreservesValidatedSerialOptions) {
  ExecutionContextOptions options;
  options.fallback = FallbackPolicy::kSameSpaceReference;
  options.determinism = DeterminismPolicy::kBestEffort;

  Result<ExecutionContext> result = ExecutionContext::Create(options);
  ASSERT_TRUE(result.ok());
  const ExecutionContext& context = result.value();
  EXPECT_EQ(context.GetBackend(), BackendKind::kSerial);
  EXPECT_EQ(context.GetFallbackPolicy(), FallbackPolicy::kSameSpaceReference);
  EXPECT_EQ(context.GetDeterminismPolicy(),
            DeterminismPolicy::kBestEffort);
}

TEST(ExecutionContextTest, RejectsInvalidSerialDeviceIdentifiers) {
  ExecutionContextOptions negative;
  negative.device_id = -1;
  Result<ExecutionContext> negative_result =
      ExecutionContext::Create(negative);
  ASSERT_FALSE(negative_result.ok());
  EXPECT_EQ(negative_result.status().code(), StatusCode::kInvalidArgument);

  ExecutionContextOptions nonzero;
  nonzero.device_id = 1;
  Result<ExecutionContext> nonzero_result =
      ExecutionContext::Create(nonzero);
  ASSERT_FALSE(nonzero_result.ok());
  EXPECT_EQ(nonzero_result.status().code(), StatusCode::kInvalidArgument);
}

TEST(ExecutionContextTest, RejectsInvalidPolicyAndBackendEnumerators) {
  ExecutionContextOptions options;
  options.fallback = static_cast<FallbackPolicy>(255);
  Result<ExecutionContext> fallback_result =
      ExecutionContext::Create(options);
  ASSERT_FALSE(fallback_result.ok());
  EXPECT_EQ(fallback_result.status().code(), StatusCode::kInvalidArgument);

  options = ExecutionContextOptions{};
  options.determinism = static_cast<DeterminismPolicy>(255);
  Result<ExecutionContext> determinism_result =
      ExecutionContext::Create(options);
  ASSERT_FALSE(determinism_result.ok());
  EXPECT_EQ(determinism_result.status().code(),
            StatusCode::kInvalidArgument);

  options = ExecutionContextOptions{};
  options.backend = static_cast<BackendKind>(255);
  Result<ExecutionContext> backend_result = ExecutionContext::Create(options);
  ASSERT_FALSE(backend_result.ok());
  EXPECT_EQ(backend_result.status().code(), StatusCode::kInvalidArgument);
}

TEST(ExecutionContextTest, ReportsUnavailableCanonicalOpenMPAndCudaProviders) {
  ExecutionContextOptions options;
  options.backend = BackendKind::kOpenMP;
  Result<ExecutionContext> openmp = ExecutionContext::Create(options);
  ASSERT_FALSE(openmp.ok());
  EXPECT_EQ(openmp.status().code(), StatusCode::kUnavailable);
  EXPECT_EQ(openmp.status().provider(), "openmp");

  options.backend = BackendKind::kCuda;
  Result<ExecutionContext> cuda = ExecutionContext::Create(options);
  ASSERT_FALSE(cuda.ok());
  EXPECT_EQ(cuda.status().code(), StatusCode::kUnavailable);
  EXPECT_EQ(cuda.status().provider(), "cuda");
}

TEST(ExecutionContextTest, ReturnsOnlyImplementedMemoryResources) {
  const ExecutionContext context = ExecutionContext::Serial();
  Result<MemoryResourcePtr> host =
      context.GetMemoryResource(MemorySpace::kHost);
  ASSERT_TRUE(host.ok());
  ASSERT_NE(host.value(), nullptr);
  EXPECT_EQ(host.value(), GetHostMemoryResource());

  for (const MemorySpace unsupported :
       {MemorySpace::kPinnedHost, MemorySpace::kDevice,
        MemorySpace::kManaged}) {
    Result<MemoryResourcePtr> resource =
        context.GetMemoryResource(unsupported);
    ASSERT_FALSE(resource.ok());
    EXPECT_EQ(resource.status().code(), StatusCode::kUnsupported);
    EXPECT_EQ(resource.status().provider(), "serial");
  }
}

TEST(ExecutionContextTest, FallbackNeverImpliesTransferOrUnsupportedResource) {
  ExecutionContextOptions options;
  options.fallback = FallbackPolicy::kSameSpaceReference;
  Result<ExecutionContext> result = ExecutionContext::Create(options);
  ASSERT_TRUE(result.ok());

  Result<MemoryResourcePtr> device =
      result.value().GetMemoryResource(MemorySpace::kDevice);
  ASSERT_FALSE(device.ok());
  EXPECT_EQ(device.status().code(), StatusCode::kUnsupported);
}

TEST(ExecutionContextTest, SynchronizeSucceedsForSerialProvider) {
  const ExecutionContext context = ExecutionContext::Serial();
  const Status first = context.Synchronize();
  const Status second = context.Synchronize();

  EXPECT_TRUE(first.ok());
  EXPECT_TRUE(second.ok());
}

TEST(ExecutionContextTest, IndependentContextsRetainIndependentOptions) {
  ExecutionContextOptions best_effort_options;
  best_effort_options.fallback = FallbackPolicy::kSameSpaceReference;
  best_effort_options.determinism = DeterminismPolicy::kBestEffort;
  Result<ExecutionContext> best_effort =
      ExecutionContext::Create(best_effort_options);
  ASSERT_TRUE(best_effort.ok());
  const ExecutionContext deterministic = ExecutionContext::Serial();

  EXPECT_EQ(best_effort.value().GetFallbackPolicy(),
            FallbackPolicy::kSameSpaceReference);
  EXPECT_EQ(best_effort.value().GetDeterminismPolicy(),
            DeterminismPolicy::kBestEffort);
  EXPECT_EQ(deterministic.GetFallbackPolicy(), FallbackPolicy::kDisallow);
  EXPECT_EQ(deterministic.GetDeterminismPolicy(),
            DeterminismPolicy::kRequireDeterministic);
}

TEST(ExecutionContextTest, ImmutableCopiesAreConcurrentlyUsable) {
  constexpr int kThreadCount = 8;
  constexpr int kIterations = 200;
  const ExecutionContext context = ExecutionContext::Serial();
  std::atomic<bool> all_calls_succeeded = true;
  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);

  for (int thread = 0; thread < kThreadCount; ++thread) {
    const ExecutionContext copy = context;
    threads.emplace_back([copy, &all_calls_succeeded] {
      for (int iteration = 0; iteration < kIterations; ++iteration) {
        Result<MemoryResourcePtr> resource =
            copy.GetMemoryResource(MemorySpace::kHost);
        const Status status = copy.Synchronize();
        if (!resource.ok() || !status.ok() ||
            copy.GetBackend() != BackendKind::kSerial) {
          all_calls_succeeded.store(false, std::memory_order_relaxed);
          return;
        }
      }
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
  EXPECT_TRUE(all_calls_succeeded.load(std::memory_order_relaxed));
}

}  // namespace
}  // namespace asc
