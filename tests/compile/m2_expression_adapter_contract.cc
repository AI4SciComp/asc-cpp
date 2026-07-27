#include <array>
#include <span>
#include <string>

#include "asc/expression/expression.h"

namespace third_party_expression_contract {

struct Unadapted {};
struct MissingAliasQuery {};
struct WrongReadType {};
struct WrongRankType {};
struct VoidValueType {};

}  // namespace third_party_expression_contract

namespace asc {

template <>
struct ExpressionAdapter<third_party_expression_contract::MissingAliasQuery> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const third_party_expression_contract::MissingAliasQuery&) {
    return {1};
  }
  static double Read(const third_party_expression_contract::MissingAliasQuery&,
                     std::span<const index_t, 1>) {
    return 0.0;
  }
};

template <>
struct ExpressionAdapter<third_party_expression_contract::WrongReadType> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const third_party_expression_contract::WrongReadType&) {
    return {1};
  }
  static std::string Read(const third_party_expression_contract::WrongReadType&,
                          std::span<const index_t, 1>) {
    return "not the declared scalar type";
  }
  static bool MayAlias(const third_party_expression_contract::WrongReadType&,
                       AliasToken) {
    return false;
  }
};

template <>
struct ExpressionAdapter<third_party_expression_contract::WrongRankType> {
  using value_type = double;
  static constexpr int kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const third_party_expression_contract::WrongRankType&) {
    return {1};
  }
  static double Read(const third_party_expression_contract::WrongRankType&,
                     std::span<const index_t, 1>) {
    return 0.0;
  }
  static bool MayAlias(const third_party_expression_contract::WrongRankType&,
                       AliasToken) {
    return false;
  }
};

template <>
struct ExpressionAdapter<third_party_expression_contract::VoidValueType> {
  using value_type = void;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const third_party_expression_contract::VoidValueType&) {
    return {1};
  }
  static void Read(const third_party_expression_contract::VoidValueType&,
                   std::span<const index_t, 1>) {}
  static bool MayAlias(const third_party_expression_contract::VoidValueType&,
                       AliasToken) {
    return false;
  }
};

}  // namespace asc

static_assert(
    !asc::ReadableExpression<third_party_expression_contract::Unadapted>);
static_assert(!asc::ReadableExpression<
              third_party_expression_contract::MissingAliasQuery>);
static_assert(
    !asc::ReadableExpression<third_party_expression_contract::WrongReadType>);
static_assert(
    !asc::ReadableExpression<third_party_expression_contract::WrongRankType>);
static_assert(
    !asc::ReadableExpression<third_party_expression_contract::VoidValueType>);

int main() { return 0; }
