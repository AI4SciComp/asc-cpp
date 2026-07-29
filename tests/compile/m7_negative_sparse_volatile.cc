#include "asc/sparse/providers/cuda.h"

void CreateVolatileVector(volatile float* data) {
  const auto view =
      asc::CudaStridedVectorView<volatile float>::Create(data, 1, 1);
  static_cast<void>(view);
}
