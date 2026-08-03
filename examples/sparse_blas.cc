#include <array>

#include "asc/core/execution.h"
#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/sparse/blas.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"

int main() {
  using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto extents = MatrixExtents::Create(2, 3);
  if (!extents.ok()) {
    return 1;
  }
  constexpr std::array<asc::index_t, 6> kCoordinates{0, 0, 0, 2, 1, 1};
  constexpr std::array<double, 3> kValues{2.0, -1.0, 4.0};
  asc::HostMemoryResource resource;
  auto coordinate = asc::CoordinateArray<double, MatrixExtents>::Create(
      resource, *extents, kCoordinates, kValues);
  if (!coordinate.ok()) {
    return 2;
  }
  auto coordinate_view = coordinate->view();
  if (!coordinate_view.ok()) {
    return 3;
  }
  auto csr = asc::ConvertToCsr<double>(asc::ExecutionContext::Serial(),
                                       *coordinate_view, resource);
  if (!csr.ok() || csr->nnz() != 3) {
    return 4;
  }

  constexpr std::array<asc::index_t, 2> kIndices{0, 2};
  constexpr std::array<double, 2> kSparseValues{2.0, -1.0};
  std::array<double, 3> dense_values{3.0, 4.0, 5.0};
  auto sparse = asc::SparseBlasIndexedVectorView<const double>::Create(
      kIndices.data(), kSparseValues.data(), 2, 3,
      {kIndices.data(), sizeof(kIndices), asc::MemorySpace::kHost},
      {kSparseValues.data(), sizeof(kSparseValues), asc::MemorySpace::kHost});
  auto dense = asc::SparseBlasVectorView<double>::Create(
      dense_values.data(), 3, 1,
      {dense_values.data(), sizeof(dense_values), asc::MemorySpace::kHost});
  if (!sparse.ok() || !dense.ok()) {
    return 5;
  }
  const asc::Status status =
      asc::SparseAxpy(asc::ExecutionContext::Serial(), 2.0, *sparse, *dense);
  return status.ok() && dense_values == std::array<double, 3>{7.0, 4.0, 3.0}
             ? 0
             : 6;
}
