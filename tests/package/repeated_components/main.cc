#include <cstddef>

#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/dense/array.h"
#include "asc/utilities/timer.h"

int main() {
  using Extents = asc::Extents<1>;
  using Array = asc::DenseArray<double, Extents>;

  auto extents = Extents::Create();
  if (!extents.ok()) {
    return 1;
  }
  asc::HostMemoryResource resource;
  auto values = Array::Create(resource, *extents);
  if (!values.ok()) {
    return 2;
  }
  asc::Timer timer;
  return values->logical_size() == 1 && timer.sample_count() == std::size_t{0}
             ? 0
             : 3;
}
