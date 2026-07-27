#include "asc/expression/expression.h"

namespace third_party_expression_negative {

struct Unadapted {};

}  // namespace third_party_expression_negative

int main() {
  const auto invalid =
      asc::MakeAdd(third_party_expression_negative::Unadapted{}, 1.0);
  return invalid.ok() ? 0 : 1;
}
