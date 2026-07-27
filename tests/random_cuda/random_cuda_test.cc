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

struct CudaFixture {
  asc::ExecutionContext execution;
  std::unique_ptr<asc::CudaMemoryResource> resource;
};

asc::Result<CudaFixture> MakeFixture() {
  auto execution =
      asc::CreateCudaExecutionContext(asc_random_cuda_test::CudaDevice());
  if (!execution.ok()) {
    return execution.status();
  }
  auto resource = asc::CudaMemoryResource::Create(
      asc_random_cuda_test::CudaDevice(), asc::MemorySpace::kDevice);
  if (!resource.ok()) {
    return resource.status();
  }
  return CudaFixture{*execution, std::move(*resource)};
}

void CheckWords(std::span<const std::uint32_t> actual, asc::RandomStream stream,
                asc::RandomSubsequence subsequence, asc::RandomOffset offset,
                TestContext& test) {
  for (std::size_t index = 0; index < actual.size(); ++index) {
    const auto address = offset + static_cast<asc::RandomOffset>(index);
    const std::uint32_t independent =
        asc_random_cuda_test::PhiloxWordOracle(stream, subsequence, address);
    ASC_M7_CUDA_EQ(test, actual[index], independent);
    ASC_M7_CUDA_EQ(test, actual[index],
                   asc::Philox4x32Word(stream, subsequence, address));
  }
}

void CheckGenerationCases(CudaFixture& fixture, TestContext& test) {
  constexpr asc::RandomStream kStream = UINT64_C(0xfedcba9876543210);
  constexpr asc::RandomSubsequence kSubsequence = UINT64_C(0x0123456789abcdef);
  constexpr asc::RandomOffset kOffset = (UINT64_C(1) << 34U) + 3U;

  for (const std::size_t count :
       {std::size_t{1}, std::size_t{3}, std::size_t{4}, std::size_t{5},
        std::size_t{1031}}) {
    std::vector<std::uint32_t> sentinels(count, UINT32_C(0xa5a5a5a5));
    auto device = asc_random_cuda_test::Upload<std::uint32_t>(
        sentinels, *fixture.resource, fixture.execution);
    ASC_M7_CUDA_CHECK(test, device.ok());
    if (!device.ok()) {
      continue;
    }
    auto generation = asc::CudaFillPhilox4x32(
        fixture.execution, static_cast<std::uint32_t*>(device->data()), count,
        kStream, kSubsequence, kOffset);
    ASC_M7_CUDA_CHECK(test, generation.ok());
    if (!generation.ok()) {
      continue;
    }
    ASC_M7_CUDA_EQ(test, generation->next_offset,
                   kOffset + static_cast<asc::RandomOffset>(count));
    asc::CudaRandomWordGeneration moved(std::move(*generation));
    ASC_M7_CUDA_CHECK(test, moved.completion.Wait().ok());
    auto result = asc_random_cuda_test::Download<std::uint32_t>(
        *device, fixture.execution);
    ASC_M7_CUDA_CHECK(test, result.ok());
    if (result.ok()) {
      CheckWords(*result, kStream, kSubsequence, kOffset, test);
    }
  }
}

void CheckZeroAndFailures(CudaFixture& fixture, TestContext& test) {
  constexpr asc::RandomStream kStream = 9;
  constexpr asc::RandomSubsequence kSubsequence = 17;
  constexpr asc::RandomOffset kOffset = 31;

  auto zero = asc::CudaFillPhilox4x32(fixture.execution, nullptr, 0, kStream,
                                      kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, zero.ok());
  if (zero.ok()) {
    ASC_M7_CUDA_EQ(test, zero->next_offset, kOffset);
    ASC_M7_CUDA_CHECK(test, zero->completion.Wait().ok());
  }

  const std::array<std::uint32_t, 4> sentinel = {
      UINT32_C(0xc001cafe), UINT32_C(0xc001cafe), UINT32_C(0xc001cafe),
      UINT32_C(0xc001cafe)};
  auto device = asc_random_cuda_test::Upload<std::uint32_t>(
      sentinel, *fixture.resource, fixture.execution);
  ASC_M7_CUDA_CHECK(test, device.ok());
  if (!device.ok()) {
    return;
  }
  auto* const destination = static_cast<std::uint32_t*>(device->data());

  const auto overrun = asc::CudaFillPhilox4x32(fixture.execution, destination,
                                               sentinel.size() + 1U, kStream,
                                               kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !overrun.ok());
  if (!overrun.ok()) {
    ASC_M7_CUDA_EQ(test, overrun.status().code(),
                   asc::ErrorCode::kMemoryAccess);
  }

  const auto overflow = asc::CudaFillPhilox4x32(
      fixture.execution, destination, 2, kStream, kSubsequence,
      std::numeric_limits<asc::RandomOffset>::max());
  ASC_M7_CUDA_CHECK(test, !overflow.ok());
  if (!overflow.ok()) {
    ASC_M7_CUDA_EQ(test, overflow.status().code(), asc::ErrorCode::kOverflow);
  }

  const auto serial =
      asc::CudaFillPhilox4x32(asc::ExecutionContext::Serial(), destination, 4,
                              kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !serial.ok());

  std::array<std::uint32_t, 4> host = sentinel;
  const auto host_destination =
      asc::CudaFillPhilox4x32(fixture.execution, host.data(), host.size(),
                              kStream, kSubsequence, kOffset);
  ASC_M7_CUDA_CHECK(test, !host_destination.ok());
  ASC_M7_CUDA_CHECK(test, host == sentinel);

  auto byte_buffer = asc::Buffer::Allocate(*fixture.resource, 8, 64);
  ASC_M7_CUDA_CHECK(test, byte_buffer.ok());
  if (byte_buffer.ok()) {
    auto* const misaligned = reinterpret_cast<std::uint32_t*>(
        static_cast<std::byte*>(byte_buffer->data()) + 1);
    const auto alignment = asc::CudaFillPhilox4x32(
        fixture.execution, misaligned, 1, kStream, kSubsequence, kOffset);
    ASC_M7_CUDA_CHECK(test, !alignment.ok());
  }

  auto unchanged =
      asc_random_cuda_test::Download<std::uint32_t>(*device, fixture.execution);
  ASC_M7_CUDA_CHECK(test, unchanged.ok());
  if (unchanged.ok()) {
    ASC_M7_CUDA_CHECK(
        test, *unchanged ==
                  std::vector<std::uint32_t>(sentinel.begin(), sentinel.end()));
  }
}

void CheckPartitionAndConcurrency(CudaFixture& first, TestContext& test) {
  constexpr std::size_t kFirstCount = 257;
  constexpr std::size_t kSecondCount = 773;
  constexpr std::size_t kCount = kFirstCount + kSecondCount;
  constexpr asc::RandomStream kStream = UINT64_C(0x1111222233334444);
  constexpr asc::RandomSubsequence kSubsequence = UINT64_C(0xaaaabbbbccccdddd);
  constexpr asc::RandomOffset kOffset = 7;

  std::vector<std::uint32_t> zero(kCount, 0);
  auto whole = asc_random_cuda_test::Upload<std::uint32_t>(
      zero, *first.resource, first.execution);
  auto partitioned = asc_random_cuda_test::Upload<std::uint32_t>(
      zero, *first.resource, first.execution);
  auto second = MakeFixture();
  ASC_M7_CUDA_CHECK(test, whole.ok());
  ASC_M7_CUDA_CHECK(test, partitioned.ok());
  ASC_M7_CUDA_CHECK(test, second.ok());
  if (!whole.ok() || !partitioned.ok() || !second.ok()) {
    return;
  }

  auto whole_generation = asc::CudaFillPhilox4x32(
      first.execution, static_cast<std::uint32_t*>(whole->data()), kCount,
      kStream, kSubsequence, kOffset);
  auto first_generation = asc::CudaFillPhilox4x32(
      first.execution, static_cast<std::uint32_t*>(partitioned->data()),
      kFirstCount, kStream, kSubsequence, kOffset);
  auto second_generation = asc::CudaFillPhilox4x32(
      second->execution,
      static_cast<std::uint32_t*>(partitioned->data()) + kFirstCount,
      kSecondCount, kStream, kSubsequence, kOffset + kFirstCount);
  ASC_M7_CUDA_CHECK(test, whole_generation.ok());
  ASC_M7_CUDA_CHECK(test, first_generation.ok());
  ASC_M7_CUDA_CHECK(test, second_generation.ok());
  if (!whole_generation.ok() || !first_generation.ok() ||
      !second_generation.ok()) {
    return;
  }

  ASC_M7_CUDA_CHECK(test, second_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, first_generation->completion.Wait().ok());
  ASC_M7_CUDA_CHECK(test, whole_generation->completion.Wait().ok());
  auto whole_result =
      asc_random_cuda_test::Download<std::uint32_t>(*whole, first.execution);
  auto partitioned_result = asc_random_cuda_test::Download<std::uint32_t>(
      *partitioned, first.execution);
  ASC_M7_CUDA_CHECK(test, whole_result.ok());
  ASC_M7_CUDA_CHECK(test, partitioned_result.ok());
  if (whole_result.ok() && partitioned_result.ok()) {
    ASC_M7_CUDA_CHECK(test, *whole_result == *partitioned_result);
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
  CheckGenerationCases(*fixture, test);
  CheckZeroAndFailures(*fixture, test);
  CheckPartitionAndConcurrency(*fixture, test);
  return test.Finish();
}
