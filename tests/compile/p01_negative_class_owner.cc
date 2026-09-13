#include "asc/core/extents.h"
#include "asc/dense/array.h"

struct UnapprovedScalar {
  double value;
};

using InvalidOwner = asc::DenseArray<UnapprovedScalar, asc::Extents<2>>;
InvalidOwner* InvalidOwnerPointer();
