#include "asc/core/extents.h"
#include "asc/dense/array.h"
#include "m3_multi_tu.h"

std::size_t M3DenseOwnerRank() {
  using MatrixExtents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  using Matrix = asc::DenseArray<double, MatrixExtents>;
  return Matrix::kRank;
}
