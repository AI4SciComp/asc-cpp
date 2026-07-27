#include <array>

#include "asc/core.h"
#include "asc/dense.h"
#include "asc/expression.h"
#include "asc/random.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"
#include "asc/sparse.h"
#include "asc/utilities.h"

int main() {
  constexpr std::array<asc::extent_t, 1> kDenseShape{2};
  const auto mapping =
      asc::DenseLayoutMapping<1>::Create(asc::LayoutLeft{}, kDenseShape);
  if (!mapping.ok()) {
    return 1;
  }
  std::array<float, 2> dense_values{};
  const auto dense_view = asc::DenseView<float, 1>::Create(
      dense_values.data(), *mapping, asc::MemorySpace::kHost);
  if (!dense_view.ok()) {
    return 2;
  }
  const auto dense_next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *dense_view, 1, 2, 3);
  if (!dense_next.ok() || *dense_next != 5) {
    return 3;
  }

  const auto sparse_extents = asc::Extents<2>::Create();
  if (!sparse_extents.ok()) {
    return 4;
  }
  asc::HostMemoryResource resource;
  const auto sparse = asc::GenerateSparseUniform01<float>(
      asc::ExecutionContext::Serial(), *sparse_extents, 1, resource, 4, 5, 6, 7,
      8, 9);
  return sparse.ok() && sparse->array.nnz() == 1 ? 0 : 5;
}
