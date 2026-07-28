#include "asc/random/sparse.h"

using Shape = asc::Extents<2, 2>;

void MustNotCompile(const asc::ExecutionContext& context, Shape shape,
                    asc::MemoryResource& resource) {
  (void)asc::GenerateSparseUniform01<int>(context, shape, 2, resource, 1, 2, 3,
                                          4, 5, 6);
}
