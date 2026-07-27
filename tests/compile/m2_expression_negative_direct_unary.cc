#include <utility>

#include "asc/expression/expression.h"

int main() {
  asc::ExpressionOwner<asc::ScalarExpression<int>> owner(
      asc::ScalarExpression<int>(7));
  const asc::UnaryExpression<asc::NegateOperation,
                             asc::ExpressionOwner<asc::ScalarExpression<int>>>
      bypass(std::move(owner), {});
  return bypass.operand().get().value();
}
