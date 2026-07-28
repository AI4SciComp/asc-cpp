#include "asc/random/providers/sparse_cuda.h"

using Shape = asc::Extents<2, 3>;
using Generation = asc::CudaSparseUniform01Generation<float, Shape>;

void CopyGeneration(const Generation& generation) {
  const Generation copy = generation;
  static_cast<void>(copy);
}
