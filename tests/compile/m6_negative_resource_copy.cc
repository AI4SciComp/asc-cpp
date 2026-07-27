#include "asc/core/providers/cuda.h"

void CopyResource(asc::CudaMemoryResource& resource) {
  asc::CudaMemoryResource copy(resource);
  static_cast<void>(copy);
}
