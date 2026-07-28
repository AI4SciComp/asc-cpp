#include "asc/random/sparse.h"

using Shape = asc::Extents<2, 2>;

void MustNotCompile(
    const asc::SparseUniform01Generation<float, Shape>& generation) {
  auto copy = generation;
  (void)copy;
}
