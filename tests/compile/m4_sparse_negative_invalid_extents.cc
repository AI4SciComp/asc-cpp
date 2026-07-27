#include "asc/sparse/coordinate.h"

struct FakeExtents {};

using InvalidBuilder = asc::CoordinateBuilder<double, FakeExtents>;

int main() { return sizeof(InvalidBuilder); }
