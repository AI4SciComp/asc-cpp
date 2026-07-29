#include "asc/dense/view.h"

void MutateConstView(asc::DenseView<const double, 1> view) {
  *view.data() = 1.0;
}
