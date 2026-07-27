#include "asc/sparse/coordinate.h"

void RemoveConst(asc::CoordinateView<const double, 1> source) {
  asc::CoordinateView<double, 1> mutable_view(source);
  static_cast<void>(mutable_view);
}

int main() { return 0; }
