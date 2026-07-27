#include <array>

#include "asc/core/types.h"
#include "asc/dense/view.h"

void MustNotCompile(asc::DenseView<const double, 1> view) {
  const std::array<asc::index_t, 1> index = {0};
  auto element = view.At(index);
  **element = 1.0;
}
