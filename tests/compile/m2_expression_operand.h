#ifndef ASC_TESTS_COMPILE_M2_EXPRESSION_OPERAND_H_
#define ASC_TESTS_COMPILE_M2_EXPRESSION_OPERAND_H_

#include <array>
#include <cstddef>
#include <span>

#include "asc/expression/expression.h"

struct M2ExternalVector {
  std::array<double, 2> values;
  const void* identity;
};

namespace asc {

template <>
struct ExpressionAdapter<::M2ExternalVector> {
  using value_type = double;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static constexpr std::array<extent_t, 1> Shape(const ::M2ExternalVector&) {
    return {2};
  }

  static constexpr double Read(const ::M2ExternalVector& expression,
                               std::span<const index_t, 1> indices) {
    return expression.values[static_cast<std::size_t>(indices[0])];
  }

  static constexpr bool MayAlias(const ::M2ExternalVector& expression,
                                 AliasToken token) {
    return expression.identity == token.identity();
  }
};

}  // namespace asc

double M2ExpressionFromFirstTranslationUnit();
double M2ExpressionFromSecondTranslationUnit();

#endif  // ASC_TESTS_COMPILE_M2_EXPRESSION_OPERAND_H_
