#include <array>
#include <span>

#include "m2_expression_operand.h"

double M2ExpressionFromSecondTranslationUnit() {
  const int identity = 0;
  auto expression = asc::MakeMultiply(
      asc::MakeNegate(M2ExternalVector{{5.0, 7.0}, &identity}), 2.0);
  if (!expression.ok()) {
    return -1.0;
  }
  constexpr std::array<asc::index_t, 1> kIndex = {0};
  return asc::ExpressionRead(*expression,
                             std::span<const asc::index_t, 1>(kIndex));
}
