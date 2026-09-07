#include <array>
#include <span>

#include "asc/core/types.h"
#include "asc/expression/expression.h"

namespace {

struct NoAdapter {};

struct IncompleteAdapter {};

}  // namespace

namespace asc {

template <>
struct ExpressionAdapter<::IncompleteAdapter> {
  using value_type = double;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const ::IncompleteAdapter& /*expression*/) {
    return {1};
  }

  static bool MayAlias(const ::IncompleteAdapter& /*expression*/,
                       AliasToken /*token*/) {
    return false;
  }
};

}  // namespace asc

namespace {

template <typename T>
concept CanNegate = requires(T value) { asc::MakeNegate(value); };

static_assert(asc::ReadableExpression<int>);
static_assert(asc::ReadableExpression<double>);
static_assert(!asc::ReadableExpression<NoAdapter>);
static_assert(!asc::ReadableExpression<IncompleteAdapter>);
static_assert(!CanNegate<NoAdapter>);

}  // namespace

int main() {
  auto expression = asc::MakeMultiply(4, 5);
  if (!expression.ok()) {
    return 1;
  }
  return asc::ExpressionRead(*expression, std::span<const asc::index_t, 0>()) ==
                 20
             ? 0
             : 2;
}
