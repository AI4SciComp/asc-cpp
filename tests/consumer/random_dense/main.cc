#include <array>

#include "asc/random/dense.h"

int main() {
  constexpr std::array<asc::extent_t, 2> kShape{2, 2};
  const auto mapping =
      asc::DenseLayoutMapping<2>::Create(asc::LayoutRight{}, kShape);
  if (!mapping.ok()) {
    return 1;
  }
  std::array<float, 4> values{};
  const auto view = asc::DenseView<float, 2>::Create(values.data(), *mapping,
                                                     asc::MemorySpace::kHost);
  if (!view.ok()) {
    return 2;
  }
  const auto next = asc::FillDenseUniform01(asc::ExecutionContext::Serial(),
                                            *view, 10, 20, 30);
  if (!next.ok() || *next != 34) {
    return 3;
  }
  return values[0] >= 0.0F && values[0] < 1.0F ? 0 : 4;
}
