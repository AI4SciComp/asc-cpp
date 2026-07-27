#include "asc/dense/providers/cuda.h"

void CopyContext(asc::DenseCudaContext& context) {
  asc::DenseCudaContext copy(context);
  static_cast<void>(copy);
}
