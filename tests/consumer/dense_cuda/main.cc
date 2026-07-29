#include <cstdint>

#include "asc/core/providers/cuda.h"
#include "asc/dense/providers/cuda.h"

int main() {
  const asc::Result<std::int32_t> count = asc::CudaDeviceCount();
  if (!count.ok()) {
    return 1;
  }
  if (*count == 0) {
    return 0;
  }
  auto execution = asc::CreateCudaExecutionContext(0);
  if (!execution.ok()) {
    return 2;
  }
  auto context = asc::DenseCudaContext::Create(*execution);
  if (!context.ok()) {
    return 3;
  }
  return context->execution_context().backend() == asc::Backend::kCuda ? 0 : 4;
}
