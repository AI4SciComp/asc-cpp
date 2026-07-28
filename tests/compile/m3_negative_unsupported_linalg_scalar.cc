#include "asc/core/execution.h"
#include "asc/dense/linalg.h"

auto UnsupportedDot(const asc::ExecutionContext& context,
                    asc::DenseView<const int, 1> left,
                    asc::DenseView<const int, 1> right) {
  return asc::Dot(context, left, right);
}
