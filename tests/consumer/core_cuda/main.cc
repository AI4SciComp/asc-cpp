#include <cstdint>

#include "asc/core/providers/cuda.h"

int main() {
  const asc::Result<std::int32_t> count = asc::CudaDeviceCount();
  if (!count.ok()) {
    return 1;
  }
  if (*count == 0) {
    return 0;
  }
  auto context = asc::CreateCudaExecutionContext(0);
  if (!context.ok()) {
    return 2;
  }
  auto event = asc::RecordCudaEvent(*context);
  if (!event.ok()) {
    return 3;
  }
  return event->Wait().ok() ? 0 : 4;
}
