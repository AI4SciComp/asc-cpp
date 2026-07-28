#include "asc/core/execution.h"
#include "asc/dense/linalg.h"

auto WrongRankDot(const asc::ExecutionContext& context,
                  asc::DenseView<const double, 2> left,
                  asc::DenseView<const double, 2> right) {
  return asc::Dot(context, left, right);
}
