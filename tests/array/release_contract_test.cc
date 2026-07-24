#include <asc/array.h>

#include <array>

int main() {
  int caught_contracts = 0;

  try {
    static_cast<void>(asc::Extents<2, 3>().GetExtent(2));
  } catch (const asc::ContractException&) {
    ++caught_contracts;
  }

  auto mapping =
      asc::LayoutLeftMapping<asc::Extents<2, 3>>::Create(
          asc::Extents<2, 3>());
  if (!mapping.ok()) {
    return 1;
  }
  try {
    static_cast<void>(mapping.value()(2, 0));
  } catch (const asc::ContractException&) {
    ++caught_contracts;
  }

  std::array<int, 6> data{};
  using View = asc::TensorView<int, asc::Extents<2, 3>>;
  auto view = View::Create(data.data(), mapping.value(), data.size());
  if (!view.ok()) {
    return 1;
  }
  try {
    static_cast<void>(view.value()(0, 3));
  } catch (const asc::ContractException&) {
    ++caught_contracts;
  }

  asc::Tensor<int, asc::Extents<2, 3>> tensor;
  try {
    static_cast<void>(tensor.GetMapping());
  } catch (const asc::ContractException&) {
    ++caught_contracts;
  }

  return caught_contracts == 4 ? 0 : 1;
}
