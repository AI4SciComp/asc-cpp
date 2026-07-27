#include <array>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"
#include "m5_random_multi_tu.h"

float M5DenseValue() {
  constexpr std::array<asc::extent_t, 1> kShape{1};
  const auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kShape);
  if (!mapping.ok()) {
    return -1.0F;
  }
  float value = -1.0F;
  const auto view = asc::DenseView<float, 1>::Create(&value, *mapping,
                                                     asc::MemorySpace::kHost);
  if (!view.ok()) {
    return -1.0F;
  }
  const auto next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2, 3);
  return next.ok() && *next == 4 ? value : -1.0F;
}
