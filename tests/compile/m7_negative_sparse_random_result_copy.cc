#include "asc/core/extents.h"
#include "asc/random/providers/sparse_cuda.h"

using Shape = asc::Extents<2, 3>;
using Generation = asc::CudaSparseUniform01Generation<double, Shape>;

void CopySparseRandomResult(Generation& generation) {
  Generation copy(generation);
  static_cast<void>(copy);
}
