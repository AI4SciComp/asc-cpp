#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/random/sparse.h"

int main() {
  const auto extents = asc::Extents<1>::Create();
  asc::HostMemoryResource resource;
  const auto result = asc::GenerateSparseUniform01<int>(
      asc::ExecutionContext::Serial(), *extents, 1, resource, 1, 2, 3, 4, 5, 6);
  return result.ok() ? 0 : 1;
}
