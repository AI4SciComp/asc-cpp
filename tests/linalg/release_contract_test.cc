#include <asc/array.h>
#include <asc/linalg.h>

#include <array>

int main() {
  using LeftExtents = asc::Extents<2>;
  using RightExtents = asc::Extents<3>;
  auto left_mapping =
      asc::LayoutLeftMapping<LeftExtents>::Create(LeftExtents());
  auto right_mapping =
      asc::LayoutLeftMapping<RightExtents>::Create(RightExtents());
  if (!left_mapping.ok() || !right_mapping.ok()) {
    return 1;
  }
  std::array<double, 2> left{1.0, 2.0};
  std::array<double, 3> right{3.0, 4.0, 5.0};
  const auto left_view = asc::TensorView<const double, LeftExtents>::Create(
      left.data(), left_mapping.value(), left.size());
  const auto right_view = asc::TensorView<double, RightExtents>::Create(
      right.data(), right_mapping.value(), right.size());
  if (!left_view.ok() || !right_view.ok()) {
    return 1;
  }

  const asc::Status status = asc::Copy(
      asc::ExecutionContext::Serial(), left_view.value(), right_view.value());
  return !status.ok() &&
                 status.code() == asc::StatusCode::kInvalidArgument &&
                 right == std::array<double, 3>{3.0, 4.0, 5.0}
             ? 0
             : 1;
}
