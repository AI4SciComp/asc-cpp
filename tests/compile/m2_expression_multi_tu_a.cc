#include <array>
#include <span>
#include <utility>

#include "m2_expression_multi_tu.h"

namespace asc_expression_multi_tu {

double EvaluateFirst() {
  Vector vector{{1.0, 2.0}};
  const auto expression = asc::MakeAdd(vector, 3.0);
  const std::array<asc::index_t, 1> index = {1};
  return asc::ReadExpression(*expression,
                             std::span<const asc::index_t, 1>(index));
}

}  // namespace asc_expression_multi_tu
