#include "asc/core/extents.h"
#include "asc/sparse/coordinate.h"

using M4Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
using M4Builder = asc::CoordinateBuilder<double, M4Shape>;

void AttemptCopy(const M4Builder& source) { M4Builder copy(source); }

int main() { return 0; }
