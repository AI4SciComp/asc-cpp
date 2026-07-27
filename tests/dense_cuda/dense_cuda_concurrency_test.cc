#include <cuda_runtime_api.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/layout.h"
#include "asc/dense/providers/cuda.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"
#include "dense/cuda/kernels_internal.h"
#include "test_support.h"

namespace {

constexpr bool CheckLaunchGeometryBoundaries() {
  using asc::internal_dense_cuda::AdvanceCudaGridStrideOrdinal;
  using asc::internal_dense_cuda::CudaLaunchBlockCount;
  using asc::internal_dense_cuda::kCudaKernelBlockSize;
  using asc::internal_dense_cuda::kCudaMaximumBlockCount;

  constexpr asc::extent_t kFormerCapacity =
      kCudaMaximumBlockCount * kCudaKernelBlockSize;
  if (CudaLaunchBlockCount(0) != 0 || CudaLaunchBlockCount(1) != 1 ||
      CudaLaunchBlockCount(kFormerCapacity - 1) != kCudaMaximumBlockCount ||
      CudaLaunchBlockCount(kFormerCapacity) != kCudaMaximumBlockCount ||
      CudaLaunchBlockCount(kFormerCapacity + 1) != kCudaMaximumBlockCount ||
      CudaLaunchBlockCount(std::numeric_limits<asc::extent_t>::max()) !=
          kCudaMaximumBlockCount) {
    return false;
  }

  asc::extent_t ordinal = 0;
  if (!AdvanceCudaGridStrideOrdinal(kFormerCapacity + 1, kFormerCapacity,
                                    ordinal) ||
      ordinal != kFormerCapacity ||
      AdvanceCudaGridStrideOrdinal(kFormerCapacity + 1, kFormerCapacity,
                                   ordinal)) {
    return false;
  }

  constexpr asc::extent_t kMaximum = std::numeric_limits<asc::extent_t>::max();
  ordinal = kMaximum - kFormerCapacity - 1;
  if (!AdvanceCudaGridStrideOrdinal(kMaximum, kFormerCapacity, ordinal) ||
      ordinal != kMaximum - 1 ||
      AdvanceCudaGridStrideOrdinal(kMaximum, kFormerCapacity, ordinal)) {
    return false;
  }
  ordinal = kMaximum - kFormerCapacity;
  return !AdvanceCudaGridStrideOrdinal(kMaximum, kFormerCapacity, ordinal) &&
         ordinal == kMaximum - kFormerCapacity;
}

static_assert(CheckLaunchGeometryBoundaries());

bool RunIndependentStream(float value) {
  const asc::Device device{asc::Backend::kCuda, 0};
  auto execution = asc::CreateCudaExecutionContext(device);
  if (!execution.ok()) {
    return false;
  }
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!provider.ok() || !resource.ok()) {
    return false;
  }

  constexpr asc::extent_t kSize = 1 << 18;
  const std::array<asc::extent_t, 1> shape = {kSize};
  const auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, shape);
  if (!mapping.ok()) {
    return false;
  }
  auto buffer = asc::Buffer::Allocate(
      **resource, static_cast<std::size_t>(kSize) * sizeof(float),
      alignof(float));
  if (!buffer.ok()) {
    return false;
  }
  auto view = asc::DenseView<float, 1>::Create(
      static_cast<float*>(buffer->data()), *mapping, asc::MemorySpace::kDevice);
  if (!view.ok()) {
    return false;
  }

  const asc::ScalarExpression<float> scalar(value);
  auto fill = asc::CudaEvaluate(*provider, *view, scalar);
  if (!fill.ok()) {
    return false;
  }
  auto scale = asc::CudaScal(*provider, 2.0F, *view);
  if (!scale.ok() || !scale->Wait().ok()) {
    return false;
  }
  if (!fill->Wait().ok()) {
    return false;
  }

  std::vector<float> result(static_cast<std::size_t>(kSize));
  auto copied = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(result.data(), result.size() * sizeof(float),
                             asc::MemorySpace::kHost),
      *buffer->const_view());
  if (!copied.ok() || !copied->Wait().ok()) {
    return false;
  }
  for (float element : result) {
    if (element != 2.0F * value) {
      return false;
    }
  }
  return true;
}

bool CheckMultiDeviceDestructionRestoresCurrentDevice() {
  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess) {
    return false;
  }
  if (device_count < 2) {
    std::cout << "SKIP: multi-device DenseCudaContext destruction/current-"
                 "device restoration requires at least two CUDA devices; "
                 "detected "
              << device_count << '\n';
    return true;
  }

  int original_device = 0;
  if (cudaGetDevice(&original_device) != cudaSuccess ||
      cudaSetDevice(1) != cudaSuccess) {
    return false;
  }
  bool succeeded = true;
  {
    const asc::Device device{asc::Backend::kCuda, 0};
    auto execution = asc::CreateCudaExecutionContext(device);
    int current_device = -1;
    succeeded = execution.ok() &&
                cudaGetDevice(&current_device) == cudaSuccess &&
                current_device == 1;
    if (succeeded) {
      {
        auto provider = asc::DenseCudaContext::Create(*execution);
        succeeded = provider.ok() &&
                    cudaGetDevice(&current_device) == cudaSuccess &&
                    current_device == 1;
        if (succeeded) {
          succeeded = cudaSetDevice(1) == cudaSuccess;
        }
      }
      succeeded = succeeded && cudaGetDevice(&current_device) == cudaSuccess &&
                  current_device == 1;
    }
  }

  int current_device = -1;
  succeeded = succeeded && cudaGetDevice(&current_device) == cudaSuccess &&
              current_device == 1;
  if (cudaSetDevice(original_device) != cudaSuccess) {
    return false;
  }
  return succeeded;
}

bool CheckSequentialCudaErrorIsolation() {
  const asc::Device device{asc::Backend::kCuda, 0};
  auto execution = asc::CreateCudaExecutionContext(device);
  auto provider = execution.ok()
                      ? asc::DenseCudaContext::Create(*execution)
                      : asc::Result<asc::DenseCudaContext>(execution.status());
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!execution.ok() || !provider.ok() || !resource.ok()) {
    std::cerr << "sequential error isolation: provider setup failed\n";
    return false;
  }

  const std::array<asc::extent_t, 1> shape = {1};
  auto mapping = asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, shape);
  auto storage =
      asc::Buffer::Allocate(**resource, sizeof(float), alignof(float));
  if (!mapping.ok() || !storage.ok()) {
    std::cerr << "sequential error isolation: storage setup failed\n";
    return false;
  }
  auto destination =
      asc::DenseView<float, 1>::Create(static_cast<float*>(storage->data()),
                                       *mapping, asc::MemorySpace::kDevice);
  if (!destination.ok()) {
    std::cerr << "sequential error isolation: view setup failed\n";
    return false;
  }

  // Leave a real CUDA Runtime error pending. A provider operation must isolate
  // that failed operation instead of attributing it to, or poisoning, the
  // subsequent valid launch.
  const cudaError_t injected_error = cudaSetDevice(-1);
  const cudaError_t pending_error = cudaPeekAtLastError();
  if (injected_error == cudaSuccess || pending_error == cudaSuccess) {
    std::cerr << "sequential error isolation: error injection did not leave a "
                 "pending error (injected="
              << static_cast<int>(injected_error)
              << ", pending=" << static_cast<int>(pending_error) << ")\n";
    return false;
  }
  const asc::ScalarExpression<float> value(6.25F);
  auto fill = asc::CudaEvaluate(*provider, *destination, value);
  if (!fill.ok() || !fill->Wait().ok()) {
    std::cerr << "sequential error isolation: valid fill failed\n";
    return false;
  }
  auto scale = asc::CudaScal(*provider, 2.0F, *destination);
  if (!scale.ok() || !scale->Wait().ok()) {
    std::cerr << "sequential error isolation: valid scale failed\n";
    return false;
  }

  float result = 0.0F;
  auto copied = asc::CopyBytes(
      *execution,
      asc::MutableMemoryView(&result, sizeof(result), asc::MemorySpace::kHost),
      *storage->const_view());
  const bool copied_ok = copied.ok() && copied->Wait().ok();
  if (!copied_ok || result != 12.5F) {
    std::cerr << "sequential error isolation: valid copy/result failed\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  asc_dense_cuda_test::TestContext test;
  ASC_DENSE_CUDA_CHECK(test, CheckLaunchGeometryBoundaries());
  std::atomic<bool> first_ok = false;
  std::atomic<bool> second_ok = false;
  std::thread first([&] { first_ok.store(RunIndependentStream(1.25F)); });
  std::thread second([&] { second_ok.store(RunIndependentStream(-3.5F)); });
  first.join();
  second.join();
  ASC_DENSE_CUDA_CHECK(test, first_ok.load());
  ASC_DENSE_CUDA_CHECK(test, second_ok.load());
  ASC_DENSE_CUDA_CHECK(test,
                       CheckMultiDeviceDestructionRestoresCurrentDevice());
  ASC_DENSE_CUDA_CHECK(test, CheckSequentialCudaErrorIsolation());
  return test.Finish();
}
