#include "asc/core/extents.h"
#include "asc/dense/array.h"

using Unsupported = asc::DenseArray<bool, asc::Extents<asc::kDynamicExtent>>;

Unsupported* MakeUnsupported();
