#include <asc/array.h>
#include <asc/linalg.h>

#include <array>

int LinalgOdrA() {
  using Extents = asc::Extents<3>;
  auto mapping = asc::LayoutLeftMapping<Extents>::Create(Extents());
  if (!mapping.ok()) {
    return -1;
  }
  std::array<double, 3> left{1.0, 2.0, 3.0};
  std::array<double, 3> right{4.0, 5.0, 6.0};
  auto left_view = asc::TensorView<const double, Extents>::Create(
      left.data(), mapping.value(), left.size());
  auto right_view = asc::TensorView<const double, Extents>::Create(
      right.data(), mapping.value(), right.size());
  if (!left_view.ok() || !right_view.ok()) {
    return -1;
  }
  auto result = asc::Dot(asc::ExecutionContext::Serial(), left_view.value(),
                         right_view.value());
  return result.ok() && result.value() == 32.0 ? 17 : -1;
}
