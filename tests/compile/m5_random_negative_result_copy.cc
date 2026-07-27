#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/random/sparse.h"

int main() {
  const auto extents = asc::Extents<1>::Create();
  asc::HostMemoryResource resource;
  auto result = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *extents, 1, resource, 1, 2, 3, 4, 5, 6);
  auto copied = *result;
  return copied.array.nnz() == 1 ? 0 : 1;
}
