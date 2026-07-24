#include <asc/array.h>

#include <array>

int ArrayOdrA() {
  using Extents = asc::Extents<2, asc::dynamic_extent>;
  auto extents = Extents::Create(3);
  if (!extents.ok()) {
    return -1;
  }
  auto mapping = asc::LayoutLeftMapping<Extents>::Create(extents.value());
  if (!mapping.ok()) {
    return -1;
  }
  std::array<int, 6> data{};
  auto view = asc::TensorView<int, Extents>::Create(
      data.data(), mapping.value(), data.size());
  if (!view.ok()) {
    return -1;
  }
  view.value()(1, 2) = 17;
  asc::TensorView<const int, Extents> const_view = view.value();
  return const_view(1, 2);
}
