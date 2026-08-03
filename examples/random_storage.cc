#include <array>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/dense/layout.h"
#include "asc/dense/view.h"
#include "asc/random/dense.h"
#include "asc/random/sparse.h"

int main() {
  constexpr std::array<asc::extent_t, 1> kDenseExtents{4};
  auto mapping = asc::DenseLayout<1>::Create(
      std::span<const asc::extent_t, 1>(kDenseExtents));
  std::array<float, 4> storage{};
  if (!mapping.ok()) {
    return 1;
  }
  auto view = asc::DenseView<float, 1>::Create(storage.data(), *mapping,
                                               asc::MemorySpace::kHost);
  if (!view.ok()) {
    return 2;
  }
  auto next =
      asc::FillDenseUniform01(asc::ExecutionContext::Serial(), *view, 1, 2, 3);
  if (!next.ok() || *next != 7) {
    return 3;
  }

  using SparseExtents = asc::Extents<2, 3>;
  auto shape = SparseExtents::Create();
  asc::HostMemoryResource resource;
  if (!shape.ok()) {
    return 4;
  }
  auto sparse = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *shape, 2, resource, 1, 2, 3, 4, 5, 6);
  return sparse.ok() && sparse->array.nnz() == 2 ? 0 : 5;
}
