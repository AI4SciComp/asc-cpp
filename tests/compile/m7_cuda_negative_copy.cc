#include "asc/sparse/providers/cuda.h"

void CopySparseCudaContext(asc::SparseCudaContext& context) {
  asc::SparseCudaContext copy(context);
  static_cast<void>(copy);
}
