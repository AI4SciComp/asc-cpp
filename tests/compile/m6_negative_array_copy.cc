#include "asc/core/extents.h"
#include "asc/dense/array.h"

using Array = asc::DenseArray<float, asc::Extents<asc::kDynamicExtent>>;

void CopyArray(Array& array) {
  Array copy(array);
  static_cast<void>(copy);
}
