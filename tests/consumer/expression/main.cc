#include <array>
#include <cstddef>
#include <span>

#include "asc/expression.h"

namespace consumer_type {

struct Vector {
  std::array<double, 2> values;
};

}  // namespace consumer_type

namespace asc {

template <>
struct ExpressionAdapter<consumer_type::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(const consumer_type::Vector&) {
    return {2};
  }
  static double Read(const consumer_type::Vector& vector,
                     std::span<const index_t, 1> index) {
    return vector.values[static_cast<std::size_t>(index[0])];
  }
  static bool MayAlias(const consumer_type::Vector&, AliasToken) {
    return false;
  }
};

}  // namespace asc

int main() {
  consumer_type::Vector vector{{2.0, 4.0}};
  const auto expression =
      asc::MakeMultiply(asc::MakeAdd(vector, 1.0).value(), 2.0);
  if (!expression.ok()) {
    return 1;
  }
  const std::array<asc::index_t, 1> index = {1};
  return asc::ReadExpression(*expression,
                             std::span<const asc::index_t, 1>(index)) == 10.0
             ? 0
             : 2;
}
