#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/random/distribution.h"
#include "asc/random/engine.h"
#include "asc/random/sparse.h"

int main() {
  using Shape = asc::Extents<2, 3>;
  auto shape = Shape::Create();
  if (!shape.ok()) {
    return 1;
  }
  asc::HostMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *shape, 2, resource, 1, 2, 3, 4, 5, 6);
  if (!generated.ok() || generated->array.nnz() != 2 ||
      generated->next_structure_offset != 15 ||
      generated->next_value_offset != 10) {
    return 2;
  }
  auto view = generated->array.view();
  if (!view.ok()) {
    return 3;
  }
  for (asc::nnz_t position = 0; position < view->nnz(); ++position) {
    auto value = view->AtStored(position);
    if (!value.ok()) {
      return 4;
    }
    const auto word_position = static_cast<asc::RandomOffset>(2 * position);
    const double expected = asc::Uniform01<double>(
        asc::GeneratePhilox4x32Word(4, 5, 6 + word_position),
        asc::GeneratePhilox4x32Word(4, 5, 7 + word_position));
    if (**value != expected) {
      return 5;
    }
  }
  return 0;
}
