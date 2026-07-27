#include <cstddef>
#include <cstdint>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"

int main() {
  const auto count = asc::CudaDeviceCount();
  if (!count.ok() || *count <= 0) {
    return 1;
  }
  const asc::Device device{asc::Backend::kCuda, 0};
  auto context = asc::CreateCudaExecutionContext(device);
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!context.ok() || !resource.ok()) {
    return 2;
  }
  auto buffer = asc::Buffer::Allocate(**resource, 64, 64);
  if (!buffer.ok()) {
    return 3;
  }
  auto event = asc::RecordCudaEvent(*context);
  if (!event.ok() || !event->Wait().ok()) {
    return 4;
  }
  const auto query = event->Query();
  return query.ok() && *query ? 0 : 5;
}
