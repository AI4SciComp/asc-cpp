#include <string>

#include "asc/core/extents.h"
#include "asc/dense/array.h"

using VectorExtents = asc::Extents<asc::kDynamicExtent>;
using InvalidOwner = asc::DenseArray<std::string, VectorExtents>;

InvalidOwner* InvalidOwnerPointer();
