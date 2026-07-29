#include "asc/expression/expression.h"

struct M2UnadaptedExpression {};

int main() {
  auto expression = asc::MakeNegate(M2UnadaptedExpression{});
  (void)expression;
  return 0;
}
