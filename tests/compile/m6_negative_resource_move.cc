#include <utility>

#include "asc/core/providers/cuda.h"

void MoveResource(asc::CudaMemoryResource& resource) {
  asc::CudaMemoryResource moved(std::move(resource));
  static_cast<void>(moved);
}
