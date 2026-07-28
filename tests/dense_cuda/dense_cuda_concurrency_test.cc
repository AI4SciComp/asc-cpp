#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"
#include "counting_resource.h"
#include "device_test_helpers.h"
#include "test_support.h"

int main() {
  if (asc_dense_cuda_test::ForceNoCudaDevice()) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }
  asc_dense_cuda_test::TestContext test;
  auto count = asc::CudaDeviceCount();
  ASC_DENSE_CUDA_CHECK(test, count.ok());
  if (!count.ok()) {
    return test.Finish();
  }
  if (*count == 0) {
    return asc_dense_cuda_test::kSkipReturnCode;
  }

  auto pinned_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device_upstream =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto execution_a = asc::CreateCudaExecutionContext(0);
  auto execution_b = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, pinned_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, device_upstream.ok());
  ASC_DENSE_CUDA_CHECK(test, execution_a.ok());
  ASC_DENSE_CUDA_CHECK(test, execution_b.ok());
  if (!pinned_upstream.ok() || !device_upstream.ok() || !execution_a.ok() ||
      !execution_b.ok()) {
    return test.Finish();
  }
  auto context_a = asc::DenseCudaContext::Create(*execution_a);
  auto context_b = asc::DenseCudaContext::Create(*execution_b);
  ASC_DENSE_CUDA_CHECK(test, context_a.ok());
  ASC_DENSE_CUDA_CHECK(test, context_b.ok());
  if (!context_a.ok() || !context_b.ok()) {
    return test.Finish();
  }

  constexpr std::size_t kElements = 1U << 22;
  constexpr std::size_t kBytes = kElements * sizeof(float);
  asc_dense_cuda_test::CountingResource pinned(**pinned_upstream);
  asc_dense_cuda_test::CountingResource device(**device_upstream);
  auto host_a = asc::Buffer::Allocate(pinned, kBytes, alignof(float));
  auto host_b = asc::Buffer::Allocate(pinned, kBytes, alignof(float));
  auto device_a = asc::Buffer::Allocate(device, kBytes, alignof(float));
  auto device_b = asc::Buffer::Allocate(device, kBytes, alignof(float));
  ASC_DENSE_CUDA_CHECK(test, host_a.ok());
  ASC_DENSE_CUDA_CHECK(test, host_b.ok());
  ASC_DENSE_CUDA_CHECK(test, device_a.ok());
  ASC_DENSE_CUDA_CHECK(test, device_b.ok());
  if (!host_a.ok() || !host_b.ok() || !device_a.ok() || !device_b.ok()) {
    return test.Finish();
  }
  std::fill_n(asc_dense_cuda_test::Data<float>(*host_a), kElements, 2.0F);
  std::fill_n(asc_dense_cuda_test::Data<float>(*host_b), kElements, -3.0F);
  ASC_DENSE_CUDA_CHECK(
      test, asc_dense_cuda_test::CopyAndWait(context_a->execution_context(),
                                             *device_a, *host_a));
  ASC_DENSE_CUDA_CHECK(
      test, asc_dense_cuda_test::CopyAndWait(context_b->execution_context(),
                                             *device_b, *host_b));

  const std::array<asc::extent_t, 1> extents{kElements};
  const std::array<asc::stride_t, 1> strides{1};
  auto view_a = asc_dense_cuda_test::MakeView(
      asc_dense_cuda_test::Data<float>(*device_a), extents, strides,
      asc::MemorySpace::kDevice);
  auto view_b = asc_dense_cuda_test::MakeView(
      asc_dense_cuda_test::Data<float>(*device_b), extents, strides,
      asc::MemorySpace::kDevice);

  const auto submit_begin = std::chrono::steady_clock::now();
  auto event_a = asc::CudaScal(*context_a, 4.0F, view_a);
  auto event_b = asc::CudaScal(*context_b, -2.0F, view_b);
  ASC_DENSE_CUDA_CHECK(test, event_a.ok());
  ASC_DENSE_CUDA_CHECK(test, event_b.ok());
  if (!event_a.ok() || !event_b.ok()) {
    return test.Finish();
  }
  auto query_a = event_a->Query();
  auto query_b = event_b->Query();
  ASC_DENSE_CUDA_CHECK(test, query_a.ok());
  ASC_DENSE_CUDA_CHECK(test, query_b.ok());

  const auto destroy_begin = std::chrono::steady_clock::now();
  auto replacement = asc::RecordCudaEvent(context_a->execution_context());
  ASC_DENSE_CUDA_CHECK(test, replacement.ok());
  if (!replacement.ok()) {
    return test.Finish();
  }
  event_a = std::move(replacement);
  const auto destroy_end = std::chrono::steady_clock::now();
  ASC_DENSE_CUDA_CHECK(test, event_a.ok());
  if (event_a.ok()) {
    ASC_DENSE_CUDA_CHECK(test, event_a->Wait().ok());
  }
  ASC_DENSE_CUDA_CHECK(test, event_b->Wait().ok());
  const auto complete = std::chrono::steady_clock::now();

  ASC_DENSE_CUDA_CHECK(
      test, asc_dense_cuda_test::CopyAndWait(context_a->execution_context(),
                                             *host_a, *device_a));
  ASC_DENSE_CUDA_CHECK(
      test, asc_dense_cuda_test::CopyAndWait(context_b->execution_context(),
                                             *host_b, *device_b));
  for (std::size_t index : {std::size_t{0}, kElements / 2, kElements - 1}) {
    ASC_DENSE_CUDA_EQ(test, asc_dense_cuda_test::Data<float>(*host_a)[index],
                      8.0F);
    ASC_DENSE_CUDA_EQ(test, asc_dense_cuda_test::Data<float>(*host_b)[index],
                      6.0F);
  }

  const auto destroy_us = std::chrono::duration_cast<std::chrono::microseconds>(
                              destroy_end - destroy_begin)
                              .count();
  const auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            complete - submit_begin)
                            .count();
  std::cout << "event_replacement_us=" << destroy_us
            << " independent_stream_completion_us=" << total_us << '\n';

  asc::DenseCudaContext moved = std::move(*context_a);
  auto moved_from = asc::CudaScal(*context_a, 1.0F, view_a);
  ASC_DENSE_CUDA_CHECK(test, !moved_from.ok());
  if (!moved_from.ok()) {
    ASC_DENSE_CUDA_EQ(test, moved_from.status().code(),
                      asc::ErrorCode::kInvalidState);
  }
  auto moved_event = asc::CudaScal(moved, 1.0F, view_a);
  ASC_DENSE_CUDA_CHECK(test, moved_event.ok());
  if (moved_event.ok()) {
    ASC_DENSE_CUDA_CHECK(test, moved_event->Wait().ok());
  }

  return test.Finish();
}
