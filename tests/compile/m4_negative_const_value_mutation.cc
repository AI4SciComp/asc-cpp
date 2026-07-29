#include "asc/sparse/coordinate.h"

void AttemptMutation(asc::CoordinateView<const double, 2> view) {
  view.values()[0] = 1.0;
}

int main() { return 0; }
