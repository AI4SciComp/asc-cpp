#include "asc/random/sparse.h"

int main() {
  const auto extents = asc::Extents<2, 3>::Create();
  if (!extents.ok()) {
    return 1;
  }
  asc::HostMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *extents, 2, resource, 10, 20, 30, 11,
      20, 40);
  if (!generated.ok() || generated->next_structure_offset != 42 ||
      generated->next_value_offset != 44) {
    return 2;
  }
  auto view = generated->array.view();
  if (!view.ok() || view->nnz() != 2) {
    return 3;
  }
  auto value = view->ValueAt(0);
  return value.ok() && **value >= 0.0 && **value < 1.0 ? 0 : 4;
}
