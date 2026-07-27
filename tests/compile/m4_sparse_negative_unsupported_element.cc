#include "asc/core/extents.h"
#include "asc/sparse/coordinate.h"

using InvalidBuilder = asc::CoordinateBuilder<bool, asc::Extents<1>>;

int main() { return sizeof(InvalidBuilder); }
