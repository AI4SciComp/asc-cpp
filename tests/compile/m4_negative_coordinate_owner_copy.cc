#include "asc/core/extents.h"
#include "asc/sparse/coordinate.h"

using M4Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using M4Owner = asc::CoordinateArray<double, M4Shape>;

void AttemptCopy(const M4Owner& source) { M4Owner copy(source); }

int main() { return 0; }
