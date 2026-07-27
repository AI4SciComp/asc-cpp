#include <asc/dense/array.h>
#include <asc/utilities/timer.h>

int main() {
  using Extents = asc::Extents<1>;
  using Array = asc::DenseArray<double, Extents>;
  const auto extents = Extents::Create();
  if (!extents.ok()) {
    return 1;
  }
  asc::HostMemoryResource resource;
  auto values = Array::Create(*extents, resource);
  if (!values.ok()) {
    return 2;
  }
  asc::Timer timer;
  return values->size() == 1 && timer.sample_count() == 0 ? 0 : 3;
}
