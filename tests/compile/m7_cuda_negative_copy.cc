#include "asc/sparse/providers/cuda.h"

void CopySparseContext(const asc::SparseCudaContext& context) {
  const asc::SparseCudaContext copy = context;
  static_cast<void>(copy);
}
