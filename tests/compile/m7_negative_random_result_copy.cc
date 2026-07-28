#include "asc/random/providers/cuda.h"

void CopyGeneration(const asc::CudaRandomWordGeneration& generation) {
  const asc::CudaRandomWordGeneration copy = generation;
  static_cast<void>(copy);
}
