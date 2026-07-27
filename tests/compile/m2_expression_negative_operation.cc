#include <array>
#include <span>

#include "asc/expression/expression.h"

namespace third_party_expression_negative {

struct ScalarWithoutNegation {};
struct Expression {};

}  // namespace third_party_expression_negative

namespace asc {

template <>
struct ExpressionAdapter<third_party_expression_negative::Expression> {
  using value_type = third_party_expression_negative::ScalarWithoutNegation;
  static constexpr rank_t kRank = 0;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kValueDependent;

  static std::array<extent_t, 0> Shape(
      const third_party_expression_negative::Expression&) {
    return {};
  }
  static value_type Read(const third_party_expression_negative::Expression&,
                         std::span<const index_t, 0>) {
    return {};
  }
  static bool MayAlias(const third_party_expression_negative::Expression&,
                       AliasToken) {
    return false;
  }
};

}  // namespace asc

int main() {
  const auto invalid =
      asc::MakeNegate(third_party_expression_negative::Expression{});
  return invalid.ok() ? 0 : 1;
}
