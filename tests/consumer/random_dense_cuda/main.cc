#include <array>
#include <cstdint>
#include <span>

#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/providers/dense_cuda.h"

int main() {
  const asc::Result<std::int32_t> count = asc::CudaDeviceCount();
  if (!count.ok()) {
    return 1;
  }
  if (*count == 0) {
    return 0;
  }
  auto execution = asc::CreateCudaExecutionContext(0);
  auto resource = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  if (!execution.ok() || !resource.ok()) {
    return 2;
  }
  constexpr std::array<asc::extent_t, 2> kExtents = {2, 3};
  auto mapping = asc::DenseLayout<2>::Create(
      std::span<const asc::extent_t, 2>(kExtents), asc::LayoutLeft{});
  auto storage =
      asc::Buffer::Allocate(**resource, 6 * sizeof(float), alignof(float));
  if (!mapping.ok() || !storage.ok()) {
    return 3;
  }
  auto view =
      asc::DenseView<float, 2>::Create(static_cast<float*>(storage->data()),
                                       *mapping, asc::MemorySpace::kDevice);
  if (!view.ok()) {
    return 4;
  }
  auto generation = asc::CudaFillDenseUniform01(*execution, *view, 1, 2, 3);
  if (!generation.ok()) {
    return 5;
  }
  return generation->completion.Wait().ok() && generation->next_offset == 9 ? 0
                                                                            : 6;
}
