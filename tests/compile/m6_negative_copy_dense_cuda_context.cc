#include "asc/dense/providers/cuda.h"

void CopyDenseCudaContext(const asc::DenseCudaContext& context) {
  const asc::DenseCudaContext copy = context;
  static_cast<void>(copy);
}
