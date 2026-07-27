#include "asc/expression/expression.h"

int main() {
  const asc::ExpressionReference<asc::ScalarExpression<int>> dangling(
      asc::ScalarExpression<int>(7));
  return dangling.get().value();
}
