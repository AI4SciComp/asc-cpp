#include <array>
#include <cstddef>
#include <iostream>
#include <span>

#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/array.h"
#include "asc/dense/providers/cuda.h"
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

  auto device_resource =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  using VectorExtents = asc::Extents<asc::kDynamicExtent>;
  auto extents = VectorExtents::Create(1024);
  ASC_DENSE_CUDA_CHECK(test, device_resource.ok());
  ASC_DENSE_CUDA_CHECK(test, extents.ok());
  if (!device_resource.ok() || !extents.ok()) {
    return test.Finish();
  }
  auto vector = asc::DenseArray<float, VectorExtents>::CreateUninitialized(
      *extents, **device_resource);
  auto execution = asc::CreateCudaExecutionContext(0);
  ASC_DENSE_CUDA_CHECK(test, vector.ok());
  ASC_DENSE_CUDA_CHECK(test, execution.ok());
  if (!vector.ok() || !execution.ok()) {
    return test.Finish();
  }
  auto context = asc::DenseCudaContext::Create(*execution);
  ASC_DENSE_CUDA_CHECK(test, context.ok());
  if (!context.ok()) {
    return test.Finish();
  }

  auto view = vector->view();
  ASC_DENSE_CUDA_CHECK(test, view.ok());
  if (!view.ok()) {
    return test.Finish();
  }
  auto fill = asc::CudaEvaluate(*context, 0.0F, *view);
  ASC_DENSE_CUDA_CHECK(test, fill.ok());
  if (!fill.ok()) {
    std::cerr << "CudaEvaluate failed: " << fill.status().ToString() << '\n';
  }
  if (fill.ok()) {
    auto query = fill->Query();
    ASC_DENSE_CUDA_CHECK(test, query.ok());
    ASC_DENSE_CUDA_CHECK(test, fill->Wait().ok());
  }

  auto pinned =
      asc::CudaMemoryResource::Create(0, asc::MemorySpace::kPinnedHost);
  ASC_DENSE_CUDA_CHECK(test, pinned.ok());
  if (!pinned.ok()) {
    return test.Finish();
  }
  auto host_copy = vector->Clone(**pinned, context->execution_context());
  ASC_DENSE_CUDA_CHECK(test, host_copy.ok());
  if (!host_copy.ok()) {
    return test.Finish();
  }
  auto host_view = host_copy->view();
  ASC_DENSE_CUDA_CHECK(test, host_view.ok());
  if (!host_view.ok()) {
    return test.Finish();
  }
  for (asc::index_t index :
       {asc::index_t{0}, asc::index_t{512}, asc::index_t{1023}}) {
    const std::array<asc::index_t, 1> coordinate{index};
    auto element = host_view->At(std::span<const asc::index_t, 1>(coordinate));
    ASC_DENSE_CUDA_CHECK(test, element.ok());
    if (element.ok()) {
      ASC_DENSE_CUDA_EQ(test, **element, 0.0F);
    }
  }
  return test.Finish();
}
