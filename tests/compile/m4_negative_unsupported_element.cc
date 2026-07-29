#include <cstddef>

#include "asc/sparse/coordinate.h"

int main() { return static_cast<int>(sizeof(asc::CoordinateView<bool, 2>)); }
