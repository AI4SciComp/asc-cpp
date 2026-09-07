#include <array>
#include <cstddef>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/types.h"
#include "asc/dense/providers/cuda.h"
#include "device_test_helpers.h"
#include "test_support.h"

// Both provider states and outstanding work span the move-assignment sequence.
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

  auto pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  auto device = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  auto old_execution = asc::CreateCudaExecutionContext(0);
  auto replacement_execution = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, pinned.ok());
  ASC_DENSE_CUDA_CHECK(test, device.ok());
  ASC_DENSE_CUDA_CHECK(test, old_execution.ok());
  ASC_DENSE_CUDA_CHECK(test, replacement_execution.ok());
  if (!pinned.ok() || !device.ok() || !old_execution.ok() ||
      !replacement_execution.ok()) {
    return test.Finish();
  }

  // Move each execution context into its provider so the DenseCudaContext is
  // the final owner of the corresponding stream state. Move assignment must
  // therefore destroy the old cuBLAS handle before releasing the old stream.
  auto context = asc::DenseCudaContext::Create(std::move(*old_execution));
  auto replacement =
      asc::DenseCudaContext::Create(std::move(*replacement_execution));
  ASC_DENSE_CUDA_CHECK(test, context.ok());
  ASC_DENSE_CUDA_CHECK(test, replacement.ok());
  if (!context.ok() || !replacement.ok()) {
    return test.Finish();
  }

  constexpr std::size_t kElements = 4096;
  constexpr std::size_t kBytes = kElements * sizeof(double);
  auto device_buffer = asc::Buffer::Allocate(**device, kBytes, alignof(double));
  auto host_buffer = asc::Buffer::Allocate(**pinned, kBytes, alignof(double));
  ASC_DENSE_CUDA_CHECK(test, device_buffer.ok());
  ASC_DENSE_CUDA_CHECK(test, host_buffer.ok());
  if (!device_buffer.ok() || !host_buffer.ok()) {
    return test.Finish();
  }
  auto view = asc_dense_cuda_test::MakeLeftView(
      asc_dense_cuda_test::Data<double>(*device_buffer),
      std::array<asc::extent_t, 1>{kElements}, asc::MemorySpace::kDevice);

  auto initialized = asc::CudaEvaluate(*context, 2.0, view);
  ASC_DENSE_CUDA_CHECK(test, initialized.ok());
  if (initialized.ok()) {
    ASC_DENSE_CUDA_CHECK(test, initialized->Wait().ok());
  }

  *context = std::move(*replacement);
  auto scaled = asc::CudaScal(*context, 3.0, view);
  ASC_DENSE_CUDA_CHECK(test, scaled.ok());
  if (scaled.ok()) {
    ASC_DENSE_CUDA_CHECK(test, scaled->Wait().ok());
  }

  *context = std::move(*context);
  auto after_self_move = asc::RecordCudaEvent(context->execution_context());
  ASC_DENSE_CUDA_CHECK(test, after_self_move.ok());
  if (after_self_move.ok()) {
    ASC_DENSE_CUDA_CHECK(test, after_self_move->Wait().ok());
  }

  ASC_DENSE_CUDA_CHECK(
      test, asc_dense_cuda_test::CopyAndWait(context->execution_context(),
                                             *host_buffer, *device_buffer));
  for (std::size_t index : {std::size_t{0}, kElements / 2, kElements - 1}) {
    ASC_DENSE_CUDA_EQ(
        test, asc_dense_cuda_test::Data<double>(*host_buffer)[index], 6.0);
  }
  return test.Finish();
}
