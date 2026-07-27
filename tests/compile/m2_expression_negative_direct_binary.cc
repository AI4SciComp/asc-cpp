#include <utility>

#include "asc/expression/expression.h"

int main() {
  using Owner = asc::ExpressionOwner<asc::ScalarExpression<int>>;
  Owner left(asc::ScalarExpression<int>(2));
  Owner right(asc::ScalarExpression<int>(3));
  const asc::BinaryExpression<asc::AddOperation, Owner, Owner> bypass(
      std::move(left), std::move(right), {});
  return bypass.left().get().value() + bypass.right().get().value();
}
