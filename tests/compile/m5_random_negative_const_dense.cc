#include <array>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"

int main() {
  constexpr std::array<asc::extent_t, 1> kShape{1};
  const auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kShape);
  const float value = 0.0F;
  const auto view = asc::DenseView<const float, 1>::Create(
      &value, *mapping, asc::MemorySpace::kHost);
  const auto result =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2, 3);
  return result.ok() ? 0 : 1;
}
