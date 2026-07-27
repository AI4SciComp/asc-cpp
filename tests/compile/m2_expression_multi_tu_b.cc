#include <array>
#include <span>
#include <utility>

#include "m2_expression_multi_tu.h"

namespace asc_expression_multi_tu {

double EvaluateSecond() {
  Vector vector{{4.0, 5.0}};
  auto negated = asc::MakeNegate(std::move(vector));
  const auto expression = asc::MakeMultiply(std::move(*negated), 2.0);
  const std::array<asc::index_t, 1> index = {0};
  return asc::ReadExpression(*expression,
                             std::span<const asc::index_t, 1>(index));
}

}  // namespace asc_expression_multi_tu
