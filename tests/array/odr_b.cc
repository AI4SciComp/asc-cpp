#include <asc/array.h>

int ArrayOdrB() {
  using Extents = asc::Extents<asc::dynamic_extent, 3>;
  using Tensor =
      asc::Tensor<int, Extents, asc::LayoutRightMapping<Extents>>;
  auto extents = Extents::Create(2);
  if (!extents.ok()) {
    return -1;
  }
  auto tensor = Tensor::Create(extents.value());
  if (!tensor.ok()) {
    return -1;
  }
  auto view = tensor.value().View();
  if (!view.ok()) {
    return -1;
  }
  view.value()(1, 2) = 25;
  auto clone = tensor.value().Clone(asc::ExecutionContext::Serial(),
                                    asc::MemorySpace::kHost);
  if (!clone.ok()) {
    return -1;
  }
  clone.value().View().value()(1, 2) = 26;
  return view.value()(1, 2);
}
