#include "asc/core/providers/cuda.h"

#include <cstdint>

int main() {
  const asc::Result<std::int32_t> device_count = asc::CudaDeviceCount();
  if (!device_count.ok()) {
    return 1;
  }
  if (*device_count == 0) {
    return 0;
  }
  auto context = asc::CreateCudaExecutionContext(0);
  if (!context.ok()) {
    return 2;
  }
  auto completion = asc::RecordCudaEvent(*context);
  return completion.ok() && completion->Wait().ok() ? 0 : 3;
}
