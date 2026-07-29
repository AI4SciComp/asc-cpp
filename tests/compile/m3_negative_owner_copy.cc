#include "asc/core/extents.h"
#include "asc/dense/array.h"

using VectorExtents = asc::Extents<asc::kDynamicExtent>;
using Vector = asc::DenseArray<double, VectorExtents>;

void CopyOwner(Vector& owner) {
  Vector forbidden_copy = owner;
  (void)forbidden_copy;
}
