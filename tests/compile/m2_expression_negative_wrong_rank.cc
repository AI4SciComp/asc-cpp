#include <array>
#include <span>

#include "m2_expression_operand.h"

int main() {
  const M2ExternalVector vector{{1.0, 2.0}, nullptr};
  const std::array<asc::index_t, 2> indices = {0, 0};
  (void)asc::ExpressionRead(vector, std::span<const asc::index_t, 2>(indices));
  return 0;
}
