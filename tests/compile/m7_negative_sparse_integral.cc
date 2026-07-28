#include "asc/random/providers/sparse_cuda.h"

using Shape = asc::Extents<2, 3>;

void GenerateIntegralSparse(const asc::ExecutionContext& context,
                            asc::MemoryResource& resource) {
  const auto result = asc::CudaGenerateSparseUniform01<int>(
      context, Shape{}, 2, resource, 1, 2, 3, 4, 5, 6);
  static_cast<void>(result);
}
