#include "asc/random/providers/cuda.h"

void CopyRandomResult(asc::CudaRandomWordGeneration& result) {
  asc::CudaRandomWordGeneration copy(result);
  static_cast<void>(copy);
}
