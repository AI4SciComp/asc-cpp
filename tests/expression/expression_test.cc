#include "asc/expression/expression.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "test_support.h"

namespace third_party_expression {

struct Matrix {
  std::array<asc::extent_t, 2> shape = {2, 3};
  std::array<double, 6> values = {};
  const void* alias_identity = this;
  int* read_count = nullptr;
  int* copy_count = nullptr;
  int* move_count = nullptr;

  Matrix() = default;

  Matrix(std::array<asc::extent_t, 2> matrix_shape,
         std::array<double, 6> matrix_values)
      : shape(matrix_shape), values(matrix_values) {}

  Matrix(const Matrix& other)
      : shape(other.shape),
        values(other.values),
        alias_identity(other.alias_identity),
        read_count(other.read_count),
        copy_count(other.copy_count),
        move_count(other.move_count) {
    if (copy_count != nullptr) {
      ++*copy_count;
    }
  }

  Matrix(Matrix&& other) noexcept
      : shape(other.shape),
        values(other.values),
        alias_identity(other.alias_identity),
        read_count(other.read_count),
        copy_count(other.copy_count),
        move_count(other.move_count) {
    if (move_count != nullptr) {
      ++*move_count;
    }
  }

  Matrix& operator=(const Matrix&) = default;
  Matrix& operator=(Matrix&&) = default;
};

struct Vector {
  std::array<asc::extent_t, 1> shape = {3};
  std::array<double, 3> values = {};
  const void* alias_identity = this;
};

struct InvalidShape {
  std::array<asc::extent_t, 1> shape = {-1};
};

struct MoveSensitiveVector {
  std::array<asc::extent_t, 1> shape = {3};
  std::array<double, 3> values = {2.0, 4.0, 6.0};

  MoveSensitiveVector() = default;
  MoveSensitiveVector(const MoveSensitiveVector&) = default;
  MoveSensitiveVector& operator=(const MoveSensitiveVector&) = default;

  MoveSensitiveVector(MoveSensitiveVector&& other) noexcept
      : shape(other.shape), values(other.values) {
    other.shape = {-1};
  }
  MoveSensitiveVector& operator=(MoveSensitiveVector&& other) noexcept {
    shape = other.shape;
    values = other.values;
    other.shape = {-1};
    return *this;
  }
};

struct Unadapted {};

}  // namespace third_party_expression

namespace asc {

template <>
struct ExpressionAdapter<third_party_expression::Matrix> {
  using value_type = double;
  static constexpr rank_t kRank = 2;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 2> Shape(
      const third_party_expression::Matrix& matrix) {
    return matrix.shape;
  }

  static double Read(const third_party_expression::Matrix& matrix,
                     std::span<const index_t, 2> indices) {
    if (matrix.read_count != nullptr) {
      ++*matrix.read_count;
    }
    const std::size_t row = static_cast<std::size_t>(indices[0]);
    const std::size_t column = static_cast<std::size_t>(indices[1]);
    return matrix
        .values[row * static_cast<std::size_t>(matrix.shape[1]) + column];
  }

  static bool MayAlias(const third_party_expression::Matrix& matrix,
                       AliasToken alias) {
    return alias == AliasToken::FromIdentity(matrix.alias_identity);
  }
};

template <>
struct ExpressionAdapter<third_party_expression::Vector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructureFiltering;

  static std::array<extent_t, 1> Shape(
      const third_party_expression::Vector& vector) {
    return vector.shape;
  }

  static double Read(const third_party_expression::Vector& vector,
                     std::span<const index_t, 1> indices) {
    return vector.values[static_cast<std::size_t>(indices[0])];
  }

  static bool MayAlias(const third_party_expression::Vector& vector,
                       AliasToken alias) {
    return alias == AliasToken::FromIdentity(vector.alias_identity);
  }
};

template <>
struct ExpressionAdapter<third_party_expression::InvalidShape> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kDestinationRequired;

  static std::array<extent_t, 1> Shape(
      const third_party_expression::InvalidShape& expression) {
    return expression.shape;
  }

  static double Read(const third_party_expression::InvalidShape&,
                     std::span<const index_t, 1>) {
    return 0.0;
  }

  static bool MayAlias(const third_party_expression::InvalidShape&,
                       AliasToken) {
    return false;
  }
};

template <>
struct ExpressionAdapter<third_party_expression::MoveSensitiveVector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(
      const third_party_expression::MoveSensitiveVector& vector) {
    return vector.shape;
  }

  static double Read(const third_party_expression::MoveSensitiveVector& vector,
                     std::span<const index_t, 1> indices) {
    return vector.values[static_cast<std::size_t>(indices[0])];
  }

  static bool MayAlias(const third_party_expression::MoveSensitiveVector&,
                       AliasToken) {
    return false;
  }
};

}  // namespace asc

namespace {

static_assert(asc::ReadableExpression<third_party_expression::Matrix>);
static_assert(asc::ReadableExpression<third_party_expression::Vector>);
static_assert(!asc::ReadableExpression<third_party_expression::Unadapted>);
static_assert(asc::kExpressionRank<third_party_expression::Matrix> == 2);
static_assert(std::is_same_v<
              asc::ExpressionValue<third_party_expression::Matrix>, double>);

void CheckExternalProtocol(asc_expression_test::TestContext& context) {
  third_party_expression::Matrix matrix({2, 3}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  const auto shape = asc::ExpressionShape(matrix);
  ASC_EXPRESSION_TEST_EQ(context, shape, (std::array<asc::extent_t, 2>{2, 3}));

  const std::array<asc::index_t, 2> index = {1, 2};
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ReadExpression(matrix, std::span<const asc::index_t, 2>(index)),
      6.0);
  ASC_EXPRESSION_TEST_CHECK(
      context, asc::MayAlias(matrix, asc::AliasToken::FromIdentity(&matrix)));
  ASC_EXPRESSION_TEST_CHECK(
      context, !asc::MayAlias(matrix, asc::AliasToken::FromIdentity(&shape)));
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::kExpressionOperationCategory<third_party_expression::Matrix>,
      asc::ExpressionOperationCategory::kTerminal);
  ASC_EXPRESSION_TEST_EQ(
      context, asc::kExpressionSparsityEffect<third_party_expression::Matrix>,
      asc::SparsityEffect::kStructurePreserving);
}

void CheckShapesAndScalarExpansion(asc_expression_test::TestContext& context) {
  third_party_expression::Matrix left({2, 3}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  third_party_expression::Matrix right({2, 3}, {6.0, 5.0, 4.0, 3.0, 2.0, 1.0});

  const auto add = asc::MakeAdd(left, right);
  ASC_EXPRESSION_TEST_CHECK(context, add.ok());
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(*add),
                         (std::array<asc::extent_t, 2>{2, 3}));

  third_party_expression::Matrix same_size_wrong_shape(
      {3, 2}, {6.0, 5.0, 4.0, 3.0, 2.0, 1.0});
  const auto wrong_shape = asc::MakeAdd(left, same_size_wrong_shape);
  ASC_EXPRESSION_TEST_CHECK(context, !wrong_shape.ok());
  ASC_EXPRESSION_TEST_EQ(context, wrong_shape.status().code(),
                         asc::ErrorCode::kShape);

  third_party_expression::Matrix singleton_axis({2, 1},
                                                {1.0, 2.0, 0.0, 0.0, 0.0, 0.0});
  const auto no_broadcast = asc::MakeAdd(left, singleton_axis);
  ASC_EXPRESSION_TEST_CHECK(context, !no_broadcast.ok());
  ASC_EXPRESSION_TEST_EQ(context, no_broadcast.status().code(),
                         asc::ErrorCode::kShape);

  third_party_expression::Vector vector;
  const auto wrong_rank = asc::MakeAdd(left, vector);
  ASC_EXPRESSION_TEST_CHECK(context, !wrong_rank.ok());
  ASC_EXPRESSION_TEST_EQ(context, wrong_rank.status().code(),
                         asc::ErrorCode::kShape);

  const auto scalar_right = asc::MakeMultiply(left, 2.0);
  const auto scalar_left = asc::MakeSubtract(10.0, left);
  ASC_EXPRESSION_TEST_CHECK(context, scalar_right.ok());
  ASC_EXPRESSION_TEST_CHECK(context, scalar_left.ok());
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(*scalar_right),
                         (std::array<asc::extent_t, 2>{2, 3}));
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(*scalar_left),
                         (std::array<asc::extent_t, 2>{2, 3}));

  const auto scalar_scalar = asc::MakeAdd(2, 3.5);
  ASC_EXPRESSION_TEST_CHECK(context, scalar_scalar.ok());
  using ScalarNode = std::remove_cvref_t<decltype(*scalar_scalar)>;
  static_assert(asc::kExpressionRank<ScalarNode> == 0);
  ASC_EXPRESSION_TEST_CHECK(context,
                            asc::ExpressionShape(*scalar_scalar).empty());
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ReadExpression(*scalar_scalar, std::span<const asc::index_t, 0>()),
      5.5);

  third_party_expression::Matrix zero_extent({0, 3},
                                             {0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
  const auto zero = asc::MakeNegate(zero_extent);
  ASC_EXPRESSION_TEST_CHECK(context, zero.ok());
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(*zero),
                         (std::array<asc::extent_t, 2>{0, 3}));

  third_party_expression::InvalidShape invalid;
  const auto negative_extent = asc::MakeNegate(invalid);
  ASC_EXPRESSION_TEST_CHECK(context, !negative_extent.ok());
  ASC_EXPRESSION_TEST_EQ(context, negative_extent.status().code(),
                         asc::ErrorCode::kShape);
}

void CheckReadsAndMetadata(asc_expression_test::TestContext& context) {
  third_party_expression::Matrix left({2, 3}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  third_party_expression::Matrix right({2, 3}, {6.0, 5.0, 4.0, 3.0, 2.0, 1.0});
  const std::array<asc::index_t, 2> index = {1, 1};
  const std::span<const asc::index_t, 2> index_span(index);

  const auto negate = asc::MakeNegate(left);
  const auto add = asc::MakeAdd(left, right);
  const auto subtract = asc::MakeSubtract(left, right);
  const auto multiply = asc::MakeMultiply(left, right);
  const auto scalar_multiply = asc::MakeMultiply(left, 3.0);
  ASC_EXPRESSION_TEST_CHECK(context, negate.ok());
  ASC_EXPRESSION_TEST_CHECK(context, add.ok());
  ASC_EXPRESSION_TEST_CHECK(context, subtract.ok());
  ASC_EXPRESSION_TEST_CHECK(context, multiply.ok());
  ASC_EXPRESSION_TEST_CHECK(context, scalar_multiply.ok());

  ASC_EXPRESSION_TEST_EQ(context, asc::ReadExpression(*negate, index_span),
                         -5.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ReadExpression(*add, index_span), 7.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ReadExpression(*subtract, index_span),
                         3.0);
  ASC_EXPRESSION_TEST_EQ(context, asc::ReadExpression(*multiply, index_span),
                         10.0);
  ASC_EXPRESSION_TEST_EQ(
      context, asc::ReadExpression(*scalar_multiply, index_span), 15.0);

  using NegateNode = std::remove_cvref_t<decltype(*negate)>;
  using AddNode = std::remove_cvref_t<decltype(*add)>;
  using SubtractNode = std::remove_cvref_t<decltype(*subtract)>;
  using MultiplyNode = std::remove_cvref_t<decltype(*multiply)>;
  using ScalarMultiplyNode = std::remove_cvref_t<decltype(*scalar_multiply)>;
  ASC_EXPRESSION_TEST_EQ(context, asc::kExpressionSparsityEffect<NegateNode>,
                         asc::SparsityEffect::kStructurePreserving);
  ASC_EXPRESSION_TEST_EQ(context, asc::kExpressionSparsityEffect<AddNode>,
                         asc::SparsityEffect::kStructureUnion);
  ASC_EXPRESSION_TEST_EQ(context, asc::kExpressionSparsityEffect<SubtractNode>,
                         asc::SparsityEffect::kStructureUnion);
  ASC_EXPRESSION_TEST_EQ(context, asc::kExpressionSparsityEffect<MultiplyNode>,
                         asc::SparsityEffect::kStructureIntersection);
  ASC_EXPRESSION_TEST_EQ(context,
                         asc::kExpressionSparsityEffect<ScalarMultiplyNode>,
                         asc::SparsityEffect::kValueDependent);
  ASC_EXPRESSION_TEST_EQ(context, asc::kExpressionOperationCategory<AddNode>,
                         asc::ExpressionOperationCategory::kPointwise);

  ASC_EXPRESSION_TEST_CHECK(
      context, asc::MayAlias(*add, asc::AliasToken::FromIdentity(&left)));
  ASC_EXPRESSION_TEST_CHECK(
      context, asc::MayAlias(*add, asc::AliasToken::FromIdentity(&right)));
  int unrelated = 0;
  ASC_EXPRESSION_TEST_CHECK(
      context, !asc::MayAlias(*add, asc::AliasToken::FromIdentity(&unrelated)));
}

void CheckCaptureAndLifetime(asc_expression_test::TestContext& context) {
  int reads = 0;
  int copies = 0;
  int moves = 0;
  third_party_expression::Matrix lvalue({2, 3}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0});
  lvalue.alias_identity = &lvalue;
  lvalue.read_count = &reads;
  lvalue.copy_count = &copies;
  lvalue.move_count = &moves;

  const auto lvalue_node = asc::MakeAdd(lvalue, 1.0);
  ASC_EXPRESSION_TEST_CHECK(context, lvalue_node.ok());
  ASC_EXPRESSION_TEST_EQ(context, reads, 0);
  ASC_EXPRESSION_TEST_EQ(context, copies, 0);
  ASC_EXPRESSION_TEST_EQ(context, moves, 0);
  lvalue.values[0] = 9.0;
  const std::array<asc::index_t, 2> first_index = {0, 0};
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ReadExpression(*lvalue_node,
                          std::span<const asc::index_t, 2>(first_index)),
      10.0);
  ASC_EXPRESSION_TEST_EQ(context, reads, 1);

  third_party_expression::Matrix rvalue({2, 3},
                                        {2.0, 4.0, 6.0, 8.0, 10.0, 12.0});
  rvalue.copy_count = &copies;
  rvalue.move_count = &moves;
  const auto owned_node = asc::MakeMultiply(std::move(rvalue), 2.0);
  ASC_EXPRESSION_TEST_CHECK(context, owned_node.ok());
  ASC_EXPRESSION_TEST_EQ(context, copies, 0);
  ASC_EXPRESSION_TEST_CHECK(context, moves > 0);
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ReadExpression(*owned_node,
                          std::span<const asc::index_t, 2>(first_index)),
      4.0);

  auto nested = [] {
    third_party_expression::Vector temporary;
    temporary.values = {1.0, 2.0, 3.0};
    auto inner = asc::MakeAdd(std::move(temporary), 4.0);
    return asc::MakeMultiply(std::move(*inner), 2.0);
  }();
  ASC_EXPRESSION_TEST_CHECK(context, nested.ok());
  const std::array<asc::index_t, 1> last_index = {2};
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ReadExpression(*nested,
                          std::span<const asc::index_t, 1>(last_index)),
      14.0);
}

void CheckNegateMoveOrdering(asc_expression_test::TestContext& context) {
  auto node = asc::MakeNegate(third_party_expression::MoveSensitiveVector{});
  ASC_EXPRESSION_TEST_CHECK(context, node.ok());
  ASC_EXPRESSION_TEST_EQ(context, asc::ExpressionShape(*node),
                         (std::array<asc::extent_t, 1>{3}));
  const std::array<asc::index_t, 1> index = {1};
  ASC_EXPRESSION_TEST_EQ(
      context,
      asc::ReadExpression(*node, std::span<const asc::index_t, 1>(index)),
      -4.0);
}

}  // namespace

int main() {
  asc_expression_test::TestContext context;
  CheckExternalProtocol(context);
  CheckShapesAndScalarExpansion(context);
  CheckReadsAndMetadata(context);
  CheckCaptureAndLifetime(context);
  CheckNegateMoveOrdering(context);
  return context.Finish();
}
