#include "m2_expression_multi_tu.h"

int main() {
  if (asc_expression_multi_tu::EvaluateFirst() != 5.0) {
    return 1;
  }
  return asc_expression_multi_tu::EvaluateSecond() == -8.0 ? 0 : 2;
}
