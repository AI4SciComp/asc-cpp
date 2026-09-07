#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/random/engine.h"
#include "asc/random/providers/cuda.h"
#include "philox_oracle.h"
#include "test_support.h"

namespace {

static_assert(!std::is_copy_constructible_v<asc::CudaRandomWordGeneration>);
static_assert(!std::is_copy_assignable_v<asc::CudaRandomWordGeneration>);
static_assert(
    std::is_nothrow_move_constructible_v<asc::CudaRandomWordGeneration>);
static_assert(std::is_nothrow_move_assignable_v<asc::CudaRandomWordGeneration>);

using asc_random_cuda_test::TestContext;

struct Fixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<Fixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::kCudaDevice);
  if (!execution.ok()) {
    return execution.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::kCudaDevice, asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return Fixture{*execution, std::move(*resource)};
}

void CheckWords(std::span<const std::uint32_t> words, asc::RandomStream stream,
                asc::RandomSubsequence subsequence, asc::RandomOffset offset,
                TestContext& test) {
  for (std::size_t index = 0; index < words.size(); ++index) {
    const asc::RandomOffset address =
        offset + static_cast<asc::RandomOffset>(index);
    ASC_M7_CUDA_EQ(
        test, words[index],
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence, address));
    ASC_M7_CUDA_EQ(test, words[index],
                   asc::GeneratePhilox4x32Word(stream, subsequence, address));
  }
}

void CheckGeneration(Fixture& fixture, TestContext& test) {
  constexpr asc::RandomStream kStream = std::uint64_t{0xfedcba9876543210};
  constexpr asc::RandomSubsequence kSubsequence =
      std::uint64_t{0x0123456789abcdef};
  constexpr asc::RandomOffset kOffset = (std::uint64_t{1} << 34U) + 3U;

  for (const std::size_t count :
       {std::size_t{1}, std::size_t{3}, std::size_t{4}, std::size_t{5},
        std::size_t{1031}}) {
    std::vector<std::uint32_t> sentinel(count, std::uint32_t{0xa5a5a5a5});
    auto device = asc_random_cuda_test::Upload<std::uint32_t>(
        sentinel, *fixture.resource, fixture.execution);
    ASC_M7_CUDA_CHECK(test, device.ok());
    if (!device.ok()) {
      continue;
    }
    auto generated = asc::CudaFillPhilox4x32(
        fixture.execution,
        asc::MutableMemoryView(device->data(), device->size(),
                               asc::MemorySpace::kDevice),
        count, kStream, kSubsequence, kOffset);
    ASC_M7_CUDA_CHECK(test, generated.ok());
    if (!generated.ok()) {
      continue;
    }
    ASC_M7_CUDA_EQ(test, generated->next_offset,
                   kOffset + static_cast<asc::RandomOffset>(count));
    asc::CudaRandomWordGeneration moved(std::move(*generated));
    ASC_M7_CUDA_CHECK(test, moved.completion.Wait().ok());
    auto result = asc_random_cuda_test::Download<std::uint32_t>(
        *device, fixture.execution);
    ASC_M7_CUDA_CHECK(test, result.ok());
    if (result.ok()) {
      CheckWords(*result, kStream, kSubsequence, kOffset, test);
    }
  }
}

void CheckFailures(Fixture& fixture, TestContext& test) {
  constexpr asc::RandomStream kStream = 9;
  constexpr asc::RandomSubsequence kSubsequence = 17;
  constexpr asc::RandomOffset kOffset = 31;
  auto zero = asc::CudaFillPhilox4x32(
      fixture.execution,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice), 0, kStream,
      kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, zero.ok());
  if (zero.ok()) {
    ASC_M7_CUDA_EQ(test, zero->next_offset, kOffset);
    ASC_M7_CUDA_CHECK(test, zero->completion.Wait().ok());
  }

  constexpr std::array<std::uint32_t, 4> kSentinel = {
      std::uint32_t{0xc001cafe}, std::uint32_t{0xc001cafe},
      std::uint32_t{0xc001cafe}, std::uint32_t{0xc001cafe}};
  auto device = asc_random_cuda_test::Upload<std::uint32_t>(
      kSentinel, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  auto* destination = static_cast<std::uint32_t*>(device->data());
  const asc::MutableMemoryView destination_view(
      destination, kSentinel.size() * sizeof(std::uint32_t),
      asc::MemorySpace::kDevice);

  const auto overrun = asc::CudaFillPhilox4x32(
      fixture.execution, destination_view, kSentinel.size() + 1U, kStream,
      kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !overrun.ok());

  const auto overflow = asc::CudaFillPhilox4x32(
      fixture.execution, destination_view, 2, kStream, kSubsequence,
      std::numeric_limits<asc::RandomOffset>::max());
  ASC_M7_CUDA_CHECK(test, !overflow.ok());
  if (!overflow.ok()) {
    ASC_M7_CUDA_EQ(test, overflow.status().code(), asc::ErrorCode::kOverflow);
  }

  const auto serial =
      asc::CudaFillPhilox4x32(asc::ExecutionContext::Serial(), destination_view,
                              kSentinel.size(), kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !serial.ok());

  auto host = kSentinel;
  const auto host_result =
      asc::CudaFillPhilox4x32(fixture.execution,
                              asc::MutableMemoryView(host.data(), sizeof(host),
                                                     asc::MemorySpace::kHost),
                              host.size(), kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !host_result.ok());
  ASC_M7_CUDA_EQ(test, host, kSentinel);

  auto bytes = asc::Buffer::Allocate(*fixture.resource, 8, 64);
  ASC_M7_CUDA_CHECK(test, bytes.ok());
  if (bytes.ok()) {
    auto* misaligned = reinterpret_cast<std::uint32_t*>(
        static_cast<std::byte*>(bytes->data()) + 1U);
    const auto result = asc::CudaFillPhilox4x32(
        fixture.execution,
        asc::MutableMemoryView(misaligned, sizeof(std::uint32_t),
                               asc::MemorySpace::kDevice),
        1, kStream, kSubsequence, kOffset);
    ASC_M7_CUDA_CHECK(test, !result.ok());
  }

  auto unchanged =
      asc_random_cuda_test::Download<std::uint32_t>(*device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, unchanged.ok());
  if (unchanged.ok()) {
    ASC_M7_CUDA_CHECK(
        test, *unchanged == std::vector<std::uint32_t>(kSentinel.begin(),
                                                       kSentinel.end()));
  }
}

// Partition shapes and independent contexts share one whole-sequence oracle.
// NOLINTNEXTLINE(readability-function-size)
void CheckPartitionsAndContexts(Fixture& fixture, TestContext& test) {
  constexpr std::size_t kFirstCount = 127;
  constexpr std::size_t kSecondCount = 1;
  constexpr std::size_t kThirdCount = 389;
  constexpr std::size_t kFourthCount = 513;
  constexpr std::size_t kCount =
      kFirstCount + kSecondCount + kThirdCount + kFourthCount;
  constexpr asc::RandomStream kStream = std::uint64_t{0x1111222233334444};
  constexpr asc::RandomSubsequence kSubsequence =
      std::uint64_t{0xaaaabbbbccccdddd};
  constexpr asc::RandomOffset kOffset = 7;

  std::vector<std::uint32_t> zeros(kCount, 0);
  auto whole = asc_random_cuda_test::Upload<std::uint32_t>(
      zeros, *fixture.resource, fixture.execution);
  auto partitioned = asc_random_cuda_test::Upload<std::uint32_t>(
      zeros, *fixture.resource, fixture.execution);
  auto second = MakeFixture();
  ASC_M7_CUDA_CHECK(test, whole.ok());
  ASC_M7_CUDA_CHECK(test, partitioned.ok());
  ASC_M7_CUDA_CHECK(test, second.ok());
  if (!whole.ok() || !partitioned.ok() || !second.ok()) {
    return;
  }

  auto whole_generation = asc::CudaFillPhilox4x32(
      fixture.execution,
      asc::MutableMemoryView(whole->data(), whole->size(),
                             asc::MemorySpace::kDevice),
      kCount, kStream, kSubsequence, kOffset);
  // Submit disjoint uneven partitions in a deliberately reordered sequence
  // across two contexts, with an explicit empty partition.
  auto fourth_generation = asc::CudaFillPhilox4x32(
      second->execution,
      asc::MutableMemoryView(static_cast<std::uint32_t*>(partitioned->data()) +
                                 kFirstCount + kSecondCount + kThirdCount,
                             kFourthCount * sizeof(std::uint32_t),
                             asc::MemorySpace::kDevice),
      kFourthCount, kStream, kSubsequence,
      kOffset + kFirstCount + kSecondCount + kThirdCount);
  auto empty_generation = asc::CudaFillPhilox4x32(
      fixture.execution,
      asc::MutableMemoryView(nullptr, 0, asc::MemorySpace::kDevice), 0, kStream,
      kSubsequence, kOffset + kFirstCount);
  auto first_generation = asc::CudaFillPhilox4x32(
      fixture.execution,
      asc::MutableMemoryView(partitioned->data(),
                             kFirstCount * sizeof(std::uint32_t),
                             asc::MemorySpace::kDevice),
      kFirstCount, kStream, kSubsequence, kOffset);
  auto second_generation = asc::CudaFillPhilox4x32(
      second->execution,
      asc::MutableMemoryView(
          static_cast<std::uint32_t*>(partitioned->data()) + kFirstCount,
          kSecondCount * sizeof(std::uint32_t), asc::MemorySpace::kDevice),
      kSecondCount, kStream, kSubsequence, kOffset + kFirstCount);
  auto third_generation = asc::CudaFillPhilox4x32(
      fixture.execution,
      asc::MutableMemoryView(static_cast<std::uint32_t*>(partitioned->data()) +
                                 kFirstCount + kSecondCount,
                             kThirdCount * sizeof(std::uint32_t),
                             asc::MemorySpace::kDevice),
      kThirdCount, kStream, kSubsequence, kOffset + kFirstCount + kSecondCount);
  ASC_M7_CUDA_CHECK(test, whole_generation.ok());
  ASC_M7_CUDA_CHECK(test, fourth_generation.ok());
  ASC_M7_CUDA_CHECK(test, empty_generation.ok());
  ASC_M7_CUDA_CHECK(test, first_generation.ok());
  ASC_M7_CUDA_CHECK(test, second_generation.ok());
  ASC_M7_CUDA_CHECK(test, third_generation.ok());
  if (!whole_generation.ok() || !fourth_generation.ok() ||
      !empty_generation.ok() || !first_generation.ok() ||
      !second_generation.ok() || !third_generation.ok()) {
    return;
  }
  ASC_M7_CUDA_EQ(test, empty_generation->next_offset, kOffset + kFirstCount);
  ASC_M7_CUDA_CHECK(test, third_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, second_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, first_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, empty_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, fourth_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, whole_generation->completion.Wait().ok());

  auto whole_result =
      asc_random_cuda_test::Download<std::uint32_t>(*whole, fixture.execution);
  auto partitioned_result = asc_random_cuda_test::Download<std::uint32_t>(
      *partitioned, fixture.execution);
  ASC_M7_CUDA_CHECK(test, whole_result.ok());
  ASC_M7_CUDA_CHECK(test, partitioned_result.ok());
  if (whole_result.ok() && partitioned_result.ok()) {
    ASC_M7_CUDA_EQ(test, *whole_result, *partitioned_result);
    CheckWords(*partitioned_result, kStream, kSubsequence, kOffset, test);
  }
}

}  // namespace

int main() {
  if (!asc_random_cuda_test::HasCudaDevice()) {
    return asc_random_cuda_test::kSkipReturnCode;
  }
  TestContext test;
  auto fixture = MakeFixture();
  ASC_M7_CUDA_CHECK(test, fixture.ok());
  if (!fixture.ok()) {
    return test.Finish();
  }
  CheckGeneration(*fixture, test);
  CheckFailures(*fixture, test);
  CheckPartitionsAndContexts(*fixture, test);
  return test.Finish();
}
