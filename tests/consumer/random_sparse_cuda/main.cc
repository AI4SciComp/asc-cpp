#include <cstdint>

#include "asc/core/extents.h"
#include "asc/core/providers/cuda.h"
#include "asc/random/providers/sparse_cuda.h"

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
  using Shape = asc::Extents<2, 3>;
  auto shape = Shape::Create();
  if (!shape.ok()) {
    return 3;
  }
  auto generation = asc::CudaGenerateSparseUniform01<float>(
      *execution, *shape, 2, **resource, 1, 2, 3, 4, 5, 6);
  if (!generation.ok()) {
    return 4;
  }
  if (!generation->completion.Wait().ok()) {
    return 5;
  }
  auto view = generation->array.view();
  return view.ok() && view->nnz() == 2 &&
                 generation->next_structure_offset == 15 &&
                 generation->next_value_offset == 8
             ? 0
             : 6;
}
