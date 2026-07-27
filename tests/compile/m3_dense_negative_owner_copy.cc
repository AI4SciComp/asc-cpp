#include "asc/core/extents.h"
#include "asc/dense/array.h"

using Owner = asc::DenseArray<double, asc::Extents<2, 2>>;

Owner MustNotCompile(const Owner& owner) { return owner; }
