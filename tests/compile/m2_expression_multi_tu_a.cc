#include <array>
#include <span>

#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "m2_expression_operand.h"

double M2ExpressionFromFirstTranslationUnit() {
  const int identity = 0;
  M2ExternalVector vector{{2.0, 4.0}, &identity};
  auto expression = asc::MakeAdd(vector, 3.0);
  if (!expression.ok()) {
    return -1.0;
  }
  constexpr std::array<asc::index_t, 1> kIndex = {1};
  return asc::ExpressionRead(*expression,
                             std::span<const asc::index_t, 1>(kIndex));
}
