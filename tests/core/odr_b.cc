#include <asc/core.h>

int CoreOdrB() {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  asc::Result<asc::MemoryResourcePtr> resource =
      context.GetMemoryResource(asc::MemorySpace::kHost);
  if (!resource.ok() || !context.Synchronize().ok()) {
    return -1;
  }

  asc::Result<int> result(7);
  return result.value();
}
