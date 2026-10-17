#include "asc/core/extents.h"
#include "asc/dense/array.h"

using InvalidOwner = asc::DenseArray<bool, asc::Extents<2>>;
InvalidOwner* InvalidOwnerPointer();
