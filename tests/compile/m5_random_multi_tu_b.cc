#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/random/sparse.h"
#include "m5_random_multi_tu.h"

double M5SparseValue() {
  const auto extents = asc::Extents<>::Create();
  if (!extents.ok()) {
    return -1.0;
  }
  asc::HostMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *extents, 1, resource, 1, 2, 3, 4, 5, 6);
  if (!generated.ok()) {
    return -1.0;
  }
  auto view = generated->array.view();
  if (!view.ok()) {
    return -1.0;
  }
  auto value = view->ValueAt(0);
  return value.ok() ? **value : -1.0;
}
