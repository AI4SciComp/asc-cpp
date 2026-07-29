#include "m2_expression_operand.h"

int main() {
  if (M2ExpressionFromFirstTranslationUnit() != 7.0) {
    return 1;
  }
  if (M2ExpressionFromSecondTranslationUnit() != -10.0) {
    return 2;
  }
  return 0;
}
