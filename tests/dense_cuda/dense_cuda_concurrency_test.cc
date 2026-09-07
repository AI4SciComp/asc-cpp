#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <thread>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/providers/cuda.h"
#include "counting_resource.h"
#include "device_test_helpers.h"
#include "test_support.h"

// Contexts, barriers, and result buffers span the complete two-thread test.
// NOLINTNEXTLINE(readability-function-size)
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
  auto host_scalar =
      asc::Buffer::Allocate(pinned, sizeof(float), alignof(float));
  auto device_scalar =
      asc::Buffer::Allocate(device, sizeof(float), alignof(float));
  ASC_DENSE_CUDA_CHECK(test, host_a.ok());
  ASC_DENSE_CUDA_CHECK(test, host_b.ok());
  ASC_DENSE_CUDA_CHECK(test, device_a.ok());
  ASC_DENSE_CUDA_CHECK(test, device_b.ok());
  ASC_DENSE_CUDA_CHECK(test, host_scalar.ok());
  ASC_DENSE_CUDA_CHECK(test, device_scalar.ok());
  if (!host_a.ok() || !host_b.ok() || !device_a.ok() || !device_b.ok() ||
      !host_scalar.ok() || !device_scalar.ok()) {
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

  auto blas_a = asc::DenseBlasVectorView<const float>::Create(
      asc_dense_cuda_test::Data<float>(*device_a), kElements, 1,
      asc::ConstMemoryView(device_a->data(), device_a->size(),
                           asc::MemorySpace::kDevice));
  auto blas_b = asc::DenseBlasVectorView<float>::Create(
      asc_dense_cuda_test::Data<float>(*device_b), kElements, 1,
      asc::ConstMemoryView(device_b->data(), device_b->size(),
                           asc::MemorySpace::kDevice));
  auto blas_scalar = asc::DenseBlasVectorView<float>::Create(
      asc_dense_cuda_test::Data<float>(*device_scalar), 1, 1,
      asc::ConstMemoryView(device_scalar->data(), device_scalar->size(),
                           asc::MemorySpace::kDevice));
  ASC_DENSE_CUDA_CHECK(test, blas_a.ok());
  ASC_DENSE_CUDA_CHECK(test, blas_b.ok());
  ASC_DENSE_CUDA_CHECK(test, blas_scalar.ok());
  if (!blas_a.ok() || !blas_b.ok() || !blas_scalar.ok()) {
    return test.Finish();
  }

  std::atomic<bool> shared_context_ok{true};
  std::thread reduction_thread([&] {
    for (std::size_t iteration = 0; iteration < 32; ++iteration) {
      auto event = asc::CudaDot(*context_a, *blas_a, *blas_a, *blas_scalar);
      if (!event.ok() || !event->Wait().ok()) {
        shared_context_ok.store(false, std::memory_order_release);
        return;
      }
    }
  });
  std::thread scaling_thread([&] {
    for (std::size_t iteration = 0; iteration < 32; ++iteration) {
      auto event = asc::CudaScal(*context_a, 1.0F, *blas_b);
      if (!event.ok() || !event->Wait().ok()) {
        shared_context_ok.store(false, std::memory_order_release);
        return;
      }
    }
  });
  reduction_thread.join();
  scaling_thread.join();
  ASC_DENSE_CUDA_CHECK(test, shared_context_ok.load(std::memory_order_acquire));
  ASC_DENSE_CUDA_CHECK(
      test, asc_dense_cuda_test::CopyAndWait(context_a->execution_context(),
                                             *host_scalar, *device_scalar));
  ASC_DENSE_CUDA_EQ(test, asc_dense_cuda_test::Data<float>(*host_scalar)[0],
                    static_cast<float>(4 * kElements));

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
