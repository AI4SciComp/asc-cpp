#include <span>

#include "asc/expression.h"

int main() {
  auto expression = asc::MakeAdd(2, 3);
  if (!expression.ok()) {
    return 1;
  }
  if (asc::ExpressionRead(*expression, std::span<const asc::index_t, 0>()) !=
      5) {
    return 2;
  }
  return 0;
}
