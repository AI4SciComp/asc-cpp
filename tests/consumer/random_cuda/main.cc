#include <cstddef>
#include <cstdint>

#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/random/providers/cuda.h"

int main() {
  const asc::Result<std::int32_t> count = asc::CudaDeviceCount();
  if (!count.ok()) {
    return 1;
  }
  if (*count == 0) {
    return 0;
  }
  auto execution = asc::CreateCudaExecutionContext(0);
  auto resource = asc::CudaMemoryResource::Create(0, asc::MemorySpace::kDevice);
  if (!execution.ok() || !resource.ok()) {
    return 2;
  }
  auto storage = asc::Buffer::Allocate(**resource, 4 * sizeof(std::uint32_t),
                                       alignof(std::uint32_t));
  if (!storage.ok()) {
    return 3;
  }
  auto destination = storage->mutable_view();
  if (!destination.ok()) {
    return 4;
  }
  auto generation =
      asc::CudaFillPhilox4x32(*execution, *destination, 4, 1, 2, 3);
  if (!generation.ok()) {
    return 5;
  }
  return generation->completion.Wait().ok() && generation->next_offset == 7 ? 0
                                                                            : 6;
}
