#include "asc/sparse/coordinate.h"

void AttemptMutation(asc::CoordinateView<double, 2> view) {
  view.coordinates()[0] = 1;
}

int main() { return 0; }
