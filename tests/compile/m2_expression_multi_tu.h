#ifndef ASC_TESTS_COMPILE_M2_EXPRESSION_MULTI_TU_H_
#define ASC_TESTS_COMPILE_M2_EXPRESSION_MULTI_TU_H_

#include <array>
#include <span>

#include "asc/expression/expression.h"

namespace asc_expression_multi_tu {

struct Vector {
  std::array<double, 2> values;
};

double EvaluateFirst();
double EvaluateSecond();

}  // namespace asc_expression_multi_tu

namespace asc {

template <>
struct ExpressionAdapter<asc_expression_multi_tu::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(const asc_expression_multi_tu::Vector&) {
    return {2};
  }

  static double Read(const asc_expression_multi_tu::Vector& vector,
                     std::span<const index_t, 1> index) {
    return vector.values[static_cast<std::size_t>(index[0])];
  }

  static bool MayAlias(const asc_expression_multi_tu::Vector&, AliasToken) {
    return false;
  }
};

}  // namespace asc

#endif  // ASC_TESTS_COMPILE_M2_EXPRESSION_MULTI_TU_H_
