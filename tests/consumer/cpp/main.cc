#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"

int main() {
  constexpr std::array<asc::extent_t, 1> kDenseExtents{2};
  auto mapping = asc::DenseLayout<1>::Create(
      std::span<const asc::extent_t, 1>(kDenseExtents));
  std::array<float, 2> values{};
  if (!mapping.ok()) {
    return 1;
  }
  auto view = asc::DenseView<float, 1>::Create(values.data(), *mapping,
                                               asc::MemorySpace::kHost);
  if (!view.ok() ||
      !asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2, 3)
           .ok()) {
    return 2;
  }

  using Shape = asc::Extents<2, 2>;
  auto shape = Shape::Create();
  asc::HostMemoryResource resource;
  if (!shape.ok()) {
    return 3;
  }
  auto generated = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *shape, 2, resource, 4, 5, 6, 7, 8, 9);
  return generated.ok() && generated->array.nnz() == 2 ? 0 : 4;
}
