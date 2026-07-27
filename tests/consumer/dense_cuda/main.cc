#include <array>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/array.h"
#include "asc/dense/providers/cuda.h"
#include "asc/expression/expression.h"

int main() {
  const asc::Device device{asc::Backend::kCuda, 0};
  auto execution = asc::CreateCudaExecutionContext(device);
  if (!execution.ok()) {
    return 1;
  }
  auto provider = asc::DenseCudaContext::Create(*execution);
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!provider.ok() || !resource.ok()) {
    return 2;
  }

  using Shape = asc::Extents<asc::kDynamicExtent>;
  const auto shape = Shape::Create(4);
  if (!shape.ok()) {
    return 3;
  }
  auto array = asc::DenseArray<float, Shape>::CreateUninitialized(
      *shape, **resource, asc::LayoutLeft{});
  if (!array.ok()) {
    return 4;
  }
  auto view = array->view();
  if (!view.ok()) {
    return 5;
  }
  const asc::ScalarExpression<float> value(2.5F);
  auto event = asc::CudaEvaluate(*provider, *view, value);
  if (!event.ok() || !event->Wait().ok()) {
    return 6;
  }

  asc::HostMemoryResource host;
  auto result = array->Clone(host, *execution);
  if (!result.ok()) {
    return 7;
  }
  auto result_view = result->view();
  if (!result_view.ok()) {
    return 8;
  }
  for (asc::index_t index = 0; index < 4; ++index) {
    const std::array<asc::index_t, 1> coordinate = {index};
    const auto element = result_view->At(coordinate);
    if (!element.ok() || **element != 2.5F) {
      return 9;
    }
  }
  return 0;
}
