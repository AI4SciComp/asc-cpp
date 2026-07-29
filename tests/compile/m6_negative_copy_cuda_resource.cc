#include "asc/core/providers/cuda.h"

void CopyCudaResource(const asc::CudaMemoryResource& resource) {
  const asc::CudaMemoryResource copy = resource;
  static_cast<void>(copy);
}
