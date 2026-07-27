#ifndef ASC_EXPRESSION_EXPRESSION_H_
#define ASC_EXPRESSION_EXPRESSION_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

enum class ExpressionOperationCategory : std::uint8_t {
  kTerminal = 0,
  kPointwise = 1,
};

enum class SparsityEffect : std::uint8_t {
  kStructurePreserving = 0,
  kStructureFiltering = 1,
  kStructureUnion = 2,
  kStructureIntersection = 3,
  kValueDependent = 4,
  kDensifying = 5,
  kDestinationRequired = 6,
};

class AliasToken {
 public:
  [[nodiscard]] static constexpr AliasToken FromIdentity(
      const void* identity) noexcept {
    return AliasToken(identity);
  }

  // Creates a half-open byte span [address, address + bytes). A zero-byte
  // span is valid, including at a null address, and never overlaps another
  // token.
  [[nodiscard]] static Result<AliasToken> FromAddressSpan(const void* address,
                                                          std::size_t bytes) {
    if (bytes != 0 && address == nullptr) {
      return Status(ErrorCode::kMemoryAccess,
                    "A nonempty alias span cannot have a null address");
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(address);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
      return Status(ErrorCode::kOverflow,
                    "An alias address span exceeds uintptr_t");
    }
    return AliasToken(address, begin, begin + bytes);
  }

  friend constexpr bool operator==(AliasToken left, AliasToken right) noexcept {
    return left.identity_ == right.identity_;
  }

  friend bool AliasTokensMayOverlap(AliasToken left, AliasToken right) noexcept;

 private:
  explicit constexpr AliasToken(const void* identity) noexcept
      : identity_(identity) {}

  constexpr AliasToken(const void* identity, std::uintptr_t begin,
                       std::uintptr_t end) noexcept
      : identity_(identity), begin_(begin), end_(end), has_span_(true) {}

  const void* identity_;
  std::uintptr_t begin_ = 0;
  std::uintptr_t end_ = 0;
  bool has_span_ = false;
};

// Reports symmetric conservative byte overlap. Identity-only tokens behave as
// address points when compared with a span and retain identity equality when
// compared with one another.
inline bool AliasTokensMayOverlap(AliasToken left, AliasToken right) noexcept {
  if ((left.has_span_ && left.begin_ == left.end_) ||
      (right.has_span_ && right.begin_ == right.end_)) {
    return false;
  }
  if (left.has_span_ && right.has_span_) {
    return left.begin_ < right.end_ && right.begin_ < left.end_;
  }
  if (left.has_span_) {
    const std::uintptr_t point =
        reinterpret_cast<std::uintptr_t>(right.identity_);
    return left.begin_ <= point && point < left.end_;
  }
  if (right.has_span_) {
    const std::uintptr_t point =
        reinterpret_cast<std::uintptr_t>(left.identity_);
    return right.begin_ <= point && point < right.end_;
  }
  return left == right;
}

// External expression types participate by explicitly specializing this
// adapter in namespace asc. A specialization supplies:
//
//   using value_type = ...;
//   static constexpr rank_t kRank = ...;
//   static constexpr ExpressionOperationCategory kOperationCategory = ...;
//   static constexpr SparsityEffect kSparsityEffect = ...;
//   static std::array<extent_t, kRank> Shape(const T&);
//   static value_type Read(const T&, std::span<const index_t, kRank>);
//   static bool MayAlias(const T&, AliasToken);
//
// Span-aware adapters should use AliasTokensMayOverlap rather than only
// operator==. Identity-only external adapters remain responsible for reporting
// possible overlap conservatively.
template <typename T>
struct ExpressionAdapter;

namespace internal_expression {

template <typename T>
using AdaptedType = std::remove_cvref_t<T>;

template <typename T>
concept HasAdapterHeader = requires {
  typename ExpressionAdapter<AdaptedType<T>>::value_type;
  requires std::is_object_v<
      typename ExpressionAdapter<AdaptedType<T>>::value_type>;
  requires std::same_as<
      std::remove_cv_t<decltype(ExpressionAdapter<AdaptedType<T>>::kRank)>,
      rank_t>;
  requires std::same_as<
      std::remove_cv_t<
          decltype(ExpressionAdapter<AdaptedType<T>>::kOperationCategory)>,
      ExpressionOperationCategory>;
  requires std::same_as<
      std::remove_cv_t<
          decltype(ExpressionAdapter<AdaptedType<T>>::kSparsityEffect)>,
      SparsityEffect>;
};

template <typename T>
  requires HasAdapterHeader<T>
inline constexpr std::size_t kAdapterRank =
    static_cast<std::size_t>(ExpressionAdapter<AdaptedType<T>>::kRank);

template <typename T>
concept HasExpressionAdapter =
    HasAdapterHeader<T> &&
    requires(const AdaptedType<T>& expression, AliasToken alias,
             std::span<const index_t, kAdapterRank<T>> indices) {
      {
        ExpressionAdapter<AdaptedType<T>>::Shape(expression)
      } -> std::same_as<std::array<extent_t, kAdapterRank<T>>>;
      {
        ExpressionAdapter<AdaptedType<T>>::Read(expression, indices)
      } -> std::same_as<typename ExpressionAdapter<AdaptedType<T>>::value_type>;
      {
        ExpressionAdapter<AdaptedType<T>>::MayAlias(expression, alias)
      } -> std::same_as<bool>;
    };

}  // namespace internal_expression

template <typename T>
concept ReadableExpression =
    (!std::is_volatile_v<std::remove_reference_t<T>>) &&
    internal_expression::HasExpressionAdapter<T>;

template <ReadableExpression Expression>
using ExpressionValue =
    typename ExpressionAdapter<std::remove_cvref_t<Expression>>::value_type;

template <ReadableExpression Expression>
inline constexpr rank_t kExpressionRank =
    ExpressionAdapter<std::remove_cvref_t<Expression>>::kRank;

template <ReadableExpression Expression>
inline constexpr ExpressionOperationCategory kExpressionOperationCategory =
    ExpressionAdapter<std::remove_cvref_t<Expression>>::kOperationCategory;

template <ReadableExpression Expression>
inline constexpr SparsityEffect kExpressionSparsityEffect =
    ExpressionAdapter<std::remove_cvref_t<Expression>>::kSparsityEffect;

template <ReadableExpression Expression>
[[nodiscard]] auto ExpressionShape(const Expression& expression) {
  return ExpressionAdapter<std::remove_cvref_t<Expression>>::Shape(expression);
}

template <ReadableExpression Expression>
[[nodiscard]] decltype(auto) ReadExpression(
    const Expression& expression,
    std::span<const index_t,
              static_cast<std::size_t>(
                  ExpressionAdapter<std::remove_cvref_t<Expression>>::kRank)>
        indices) {
  return ExpressionAdapter<std::remove_cvref_t<Expression>>::Read(expression,
                                                                  indices);
}

template <ReadableExpression Expression>
[[nodiscard]] bool MayAlias(const Expression& expression, AliasToken alias) {
  return ExpressionAdapter<std::remove_cvref_t<Expression>>::MayAlias(
      expression, alias);
}

template <typename Scalar>
  requires std::is_arithmetic_v<Scalar>
class ScalarExpression {
 public:
  explicit constexpr ScalarExpression(Scalar value) noexcept : value_(value) {}

  [[nodiscard]] constexpr Scalar value() const noexcept { return value_; }

 private:
  Scalar value_;
};

template <typename Scalar>
struct ExpressionAdapter<ScalarExpression<Scalar>> {
  using value_type = Scalar;
  static constexpr rank_t kRank = 0;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kValueDependent;

  [[nodiscard]] static constexpr std::array<extent_t, 0> Shape(
      const ScalarExpression<Scalar>&) noexcept {
    return {};
  }

  [[nodiscard]] static constexpr Scalar Read(
      const ScalarExpression<Scalar>& expression,
      std::span<const index_t, 0>) noexcept {
    return expression.value();
  }

  [[nodiscard]] static constexpr bool MayAlias(const ScalarExpression<Scalar>&,
                                               AliasToken) noexcept {
    return false;
  }
};

template <ReadableExpression Expression>
class ExpressionReference {
 public:
  explicit constexpr ExpressionReference(const Expression& expression) noexcept
      : expression_(&expression) {}
  ExpressionReference(Expression&&) = delete;
  ExpressionReference(const Expression&&) = delete;

  [[nodiscard]] constexpr const Expression& get() const noexcept {
    return *expression_;
  }

 private:
  const Expression* expression_;
};

template <ReadableExpression Expression>
class ExpressionOwner {
 public:
  explicit constexpr ExpressionOwner(Expression expression) noexcept(
      std::is_nothrow_move_constructible_v<Expression>)
      : expression_(std::move(expression)) {}

  [[nodiscard]] constexpr const Expression& get() const noexcept {
    return expression_;
  }

 private:
  Expression expression_;
};

struct NegateOperation {
  template <typename Value>
  [[nodiscard]] constexpr auto operator()(Value&& value) const
      noexcept(noexcept(-std::forward<Value>(value)))
          -> decltype(-std::forward<Value>(value)) {
    return -std::forward<Value>(value);
  }
};

struct AddOperation {
  template <typename Left, typename Right>
  [[nodiscard]] constexpr auto operator()(Left&& left, Right&& right) const
      noexcept(noexcept(std::forward<Left>(left) + std::forward<Right>(right)))
          -> decltype(std::forward<Left>(left) + std::forward<Right>(right)) {
    return std::forward<Left>(left) + std::forward<Right>(right);
  }
};

struct SubtractOperation {
  template <typename Left, typename Right>
  [[nodiscard]] constexpr auto operator()(Left&& left, Right&& right) const
      noexcept(noexcept(std::forward<Left>(left) - std::forward<Right>(right)))
          -> decltype(std::forward<Left>(left) - std::forward<Right>(right)) {
    return std::forward<Left>(left) - std::forward<Right>(right);
  }
};

struct MultiplyOperation {
  template <typename Left, typename Right>
  [[nodiscard]] constexpr auto operator()(Left&& left, Right&& right) const
      noexcept(noexcept(std::forward<Left>(left) * std::forward<Right>(right)))
          -> decltype(std::forward<Left>(left) * std::forward<Right>(right)) {
    return std::forward<Left>(left) * std::forward<Right>(right);
  }
};

namespace internal_expression {

struct NodeFactory;

}  // namespace internal_expression

template <typename Operation, typename Operand>
class UnaryExpression {
 public:
  static constexpr std::size_t kRank = static_cast<std::size_t>(
      ExpressionAdapter<std::remove_cvref_t<
          decltype(std::declval<const Operand&>().get())>>::kRank);
  using ShapeType = std::array<extent_t, kRank>;

  [[nodiscard]] constexpr const Operand& operand() const noexcept {
    return operand_;
  }
  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return shape_;
  }

 private:
  friend struct internal_expression::NodeFactory;

  constexpr UnaryExpression(Operand operand, ShapeType shape) noexcept(
      std::is_nothrow_move_constructible_v<Operand>)
      : operand_(std::move(operand)), shape_(shape) {}

  Operand operand_;
  ShapeType shape_;
};

template <typename Operation, typename Left, typename Right>
class BinaryExpression {
 public:
  using LeftExpression =
      std::remove_cvref_t<decltype(std::declval<const Left&>().get())>;
  using RightExpression =
      std::remove_cvref_t<decltype(std::declval<const Right&>().get())>;
  static constexpr rank_t kLeftRank = ExpressionAdapter<LeftExpression>::kRank;
  static constexpr rank_t kRightRank =
      ExpressionAdapter<RightExpression>::kRank;
  static constexpr rank_t kResultRank = kLeftRank == 0 ? kRightRank : kLeftRank;
  using ShapeType = std::array<extent_t, static_cast<std::size_t>(kResultRank)>;

  [[nodiscard]] constexpr const Left& left() const noexcept { return left_; }
  [[nodiscard]] constexpr const Right& right() const noexcept { return right_; }
  [[nodiscard]] constexpr const ShapeType& shape() const noexcept {
    return shape_;
  }

 private:
  friend struct internal_expression::NodeFactory;

  constexpr BinaryExpression(Left left, Right right, ShapeType shape) noexcept(
      std::is_nothrow_move_constructible_v<Left> &&
      std::is_nothrow_move_constructible_v<Right>)
      : left_(std::move(left)), right_(std::move(right)), shape_(shape) {}

  Left left_;
  Right right_;
  ShapeType shape_;
};

namespace internal_expression {

struct NodeFactory {
  template <typename Operation, typename Operand>
  [[nodiscard]] static constexpr auto MakeUnary(
      Operand operand,
      typename UnaryExpression<Operation, Operand>::ShapeType shape) {
    return UnaryExpression<Operation, Operand>(std::move(operand), shape);
  }

  template <typename Operation, typename Left, typename Right>
  [[nodiscard]] static constexpr auto MakeBinary(
      Left left, Right right,
      typename BinaryExpression<Operation, Left, Right>::ShapeType shape) {
    return BinaryExpression<Operation, Left, Right>(std::move(left),
                                                    std::move(right), shape);
  }
};

template <typename T>
concept ExpressionOperand = std::is_arithmetic_v<std::remove_cvref_t<T>> ||
                            ReadableExpression<std::remove_cvref_t<T>>;

template <typename T, bool = std::is_arithmetic_v<std::remove_cvref_t<T>>>
struct NormalizedExpressionType {
  using type = std::remove_cvref_t<T>;
};

template <typename T>
struct NormalizedExpressionType<T, true> {
  using type = ScalarExpression<std::remove_cvref_t<T>>;
};

template <typename T>
using NormalizedExpression = typename NormalizedExpressionType<T>::type;

template <typename Operation, typename Operand,
          bool = std::is_invocable_v<
              Operation, ExpressionValue<NormalizedExpression<Operand>>>>
struct IsValidUnaryOperation : std::false_type {};

template <typename Operation, typename Operand>
struct IsValidUnaryOperation<Operation, Operand, true>
    : std::bool_constant<
          !std::is_void_v<std::remove_cvref_t<std::invoke_result_t<
              Operation, ExpressionValue<NormalizedExpression<Operand>>>>>> {};

template <typename Operation, typename Left, typename Right,
          bool = std::is_invocable_v<
              Operation, ExpressionValue<NormalizedExpression<Left>>,
              ExpressionValue<NormalizedExpression<Right>>>>
struct IsValidBinaryOperation : std::false_type {};

template <typename Operation, typename Left, typename Right>
struct IsValidBinaryOperation<Operation, Left, Right, true>
    : std::bool_constant<
          !std::is_void_v<std::remove_cvref_t<std::invoke_result_t<
              Operation, ExpressionValue<NormalizedExpression<Left>>,
              ExpressionValue<NormalizedExpression<Right>>>>>> {};

template <typename Operation, typename Operand>
concept ValidUnaryOperation = IsValidUnaryOperation<Operation, Operand>::value;

template <typename Operation, typename Left, typename Right>
concept ValidBinaryOperation =
    IsValidBinaryOperation<Operation, Left, Right>::value;

template <typename T>
using ExpressionCapture = std::conditional_t<
    std::is_arithmetic_v<std::remove_cvref_t<T>>,
    ExpressionOwner<NormalizedExpression<T>>,
    std::conditional_t<std::is_lvalue_reference_v<T>,
                       ExpressionReference<NormalizedExpression<T>>,
                       ExpressionOwner<NormalizedExpression<T>>>>;

template <ExpressionOperand Expression>
[[nodiscard]] constexpr auto Capture(Expression&& expression) {
  using CaptureType = ExpressionCapture<Expression&&>;
  if constexpr (std::is_arithmetic_v<std::remove_cvref_t<Expression>>) {
    return CaptureType(
        ScalarExpression<std::remove_cvref_t<Expression>>(expression));
  } else if constexpr (std::is_lvalue_reference_v<Expression&&>) {
    return CaptureType(expression);
  } else {
    return CaptureType(std::forward<Expression>(expression));
  }
}

template <std::size_t Rank>
[[nodiscard]] Status ValidateShape(const std::array<extent_t, Rank>& shape) {
  for (extent_t extent : shape) {
    if (extent < 0) {
      return Status(ErrorCode::kShape,
                    "An expression extent cannot be negative");
    }
  }
  return Status::Ok();
}

template <std::size_t LeftRank, std::size_t RightRank>
[[nodiscard]] Status ValidateBinaryShape(
    const std::array<extent_t, LeftRank>& left_shape,
    const std::array<extent_t, RightRank>& right_shape) {
  const Status left_status = ValidateShape(left_shape);
  if (!left_status.ok()) {
    return left_status;
  }
  const Status right_status = ValidateShape(right_shape);
  if (!right_status.ok()) {
    return right_status;
  }
  if constexpr (LeftRank == 0 || RightRank == 0) {
    return Status::Ok();
  } else if constexpr (LeftRank != RightRank) {
    return Status(ErrorCode::kShape, "Pointwise expression ranks must match");
  } else {
    if (left_shape != right_shape) {
      return Status(ErrorCode::kShape,
                    "Pointwise expression shapes must match exactly");
    }
    return Status::Ok();
  }
}

template <ReadableExpression Expression, std::size_t ResultRank>
[[nodiscard]] auto ReadOperand(const Expression& expression,
                               std::span<const index_t, ResultRank> indices) {
  if constexpr (kExpressionRank<Expression> == 0) {
    static_cast<void>(indices);
    return ReadExpression(expression, std::span<const index_t, 0>());
  } else {
    static_assert(kExpressionRank<Expression> == ResultRank);
    return ReadExpression(expression, indices);
  }
}

template <typename Operation>
inline constexpr SparsityEffect kUnarySparsityEffect =
    SparsityEffect::kValueDependent;

template <>
inline constexpr SparsityEffect kUnarySparsityEffect<NegateOperation> =
    SparsityEffect::kStructurePreserving;

template <typename Operation>
inline constexpr SparsityEffect kBinarySparsityEffect =
    SparsityEffect::kValueDependent;

template <>
inline constexpr SparsityEffect kBinarySparsityEffect<AddOperation> =
    SparsityEffect::kStructureUnion;

template <>
inline constexpr SparsityEffect kBinarySparsityEffect<SubtractOperation> =
    SparsityEffect::kStructureUnion;

template <>
inline constexpr SparsityEffect kBinarySparsityEffect<MultiplyOperation> =
    SparsityEffect::kStructureIntersection;

}  // namespace internal_expression

template <typename Operation, typename Operand>
struct ExpressionAdapter<UnaryExpression<Operation, Operand>> {
  using Expression =
      std::remove_cvref_t<decltype(std::declval<const Operand&>().get())>;
  using value_type = std::remove_cvref_t<decltype(Operation{}(
      std::declval<ExpressionValue<Expression>>()))>;
  static constexpr rank_t kRank = kExpressionRank<Expression>;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kPointwise;
  static constexpr SparsityEffect kSparsityEffect =
      internal_expression::kUnarySparsityEffect<Operation>;

  [[nodiscard]] static auto Shape(
      const UnaryExpression<Operation, Operand>& expression) {
    return expression.shape();
  }

  [[nodiscard]] static value_type Read(
      const UnaryExpression<Operation, Operand>& expression,
      std::span<const index_t, static_cast<std::size_t>(kRank)> indices) {
    return Operation{}(ReadExpression(expression.operand().get(), indices));
  }

  [[nodiscard]] static bool MayAlias(
      const UnaryExpression<Operation, Operand>& expression, AliasToken alias) {
    return asc::MayAlias(expression.operand().get(), alias);
  }
};

template <typename Operation, typename Left, typename Right>
struct ExpressionAdapter<BinaryExpression<Operation, Left, Right>> {
  using LeftExpression =
      std::remove_cvref_t<decltype(std::declval<const Left&>().get())>;
  using RightExpression =
      std::remove_cvref_t<decltype(std::declval<const Right&>().get())>;
  using value_type = std::remove_cvref_t<decltype(Operation{}(
      std::declval<ExpressionValue<LeftExpression>>(),
      std::declval<ExpressionValue<RightExpression>>()))>;
  static constexpr rank_t kLeftRank = kExpressionRank<LeftExpression>;
  static constexpr rank_t kRightRank = kExpressionRank<RightExpression>;
  static constexpr rank_t kRank = kLeftRank == 0 ? kRightRank : kLeftRank;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kPointwise;
  static constexpr SparsityEffect kSparsityEffect =
      (kLeftRank == 0 || kRightRank == 0)
          ? SparsityEffect::kValueDependent
          : internal_expression::kBinarySparsityEffect<Operation>;

  [[nodiscard]] static auto Shape(
      const BinaryExpression<Operation, Left, Right>& expression) {
    return expression.shape();
  }

  [[nodiscard]] static value_type Read(
      const BinaryExpression<Operation, Left, Right>& expression,
      std::span<const index_t, static_cast<std::size_t>(kRank)> indices) {
    return Operation{}(
        internal_expression::ReadOperand(expression.left().get(), indices),
        internal_expression::ReadOperand(expression.right().get(), indices));
  }

  [[nodiscard]] static bool MayAlias(
      const BinaryExpression<Operation, Left, Right>& expression,
      AliasToken alias) {
    return asc::MayAlias(expression.left().get(), alias) ||
           asc::MayAlias(expression.right().get(), alias);
  }
};

template <typename Operand>
  requires internal_expression::ExpressionOperand<Operand> &&
           internal_expression::ValidUnaryOperation<NegateOperation, Operand>
[[nodiscard]] auto MakeNegate(Operand&& operand) {
  auto captured = internal_expression::Capture(std::forward<Operand>(operand));
  using Capture = decltype(captured);
  using Node = UnaryExpression<NegateOperation, Capture>;
  typename Node::ShapeType shape = ExpressionShape(captured.get());
  const Status shape_status = internal_expression::ValidateShape(shape);
  if (!shape_status.ok()) {
    return Result<Node>(shape_status);
  }
  return Result<Node>(
      internal_expression::NodeFactory::MakeUnary<NegateOperation>(
          std::move(captured), shape));
}

namespace internal_expression {

template <typename Operation, typename Left, typename Right>
  requires ExpressionOperand<Left> && ExpressionOperand<Right> &&
           ValidBinaryOperation<Operation, Left, Right>
[[nodiscard]] auto MakeBinary(Left&& left, Right&& right) {
  auto captured_left = Capture(std::forward<Left>(left));
  auto captured_right = Capture(std::forward<Right>(right));
  using LeftCapture = decltype(captured_left);
  using RightCapture = decltype(captured_right);
  using Node = BinaryExpression<Operation, LeftCapture, RightCapture>;

  const auto left_shape = ExpressionShape(captured_left.get());
  const auto right_shape = ExpressionShape(captured_right.get());
  const Status shape_status = ValidateBinaryShape(left_shape, right_shape);
  if (!shape_status.ok()) {
    return Result<Node>(shape_status);
  }

  typename Node::ShapeType shape{};
  if constexpr (kExpressionRank<decltype(captured_left.get())> == 0) {
    shape = right_shape;
  } else {
    shape = left_shape;
  }
  return Result<Node>(NodeFactory::MakeBinary<Operation>(
      std::move(captured_left), std::move(captured_right), shape));
}

}  // namespace internal_expression

template <typename Left, typename Right>
  requires internal_expression::ExpressionOperand<Left> &&
           internal_expression::ExpressionOperand<Right> &&
           internal_expression::ValidBinaryOperation<AddOperation, Left, Right>
[[nodiscard]] auto MakeAdd(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<AddOperation>(
      std::forward<Left>(left), std::forward<Right>(right));
}

template <typename Left, typename Right>
  requires internal_expression::ExpressionOperand<Left> &&
           internal_expression::ExpressionOperand<Right> &&
           internal_expression::ValidBinaryOperation<SubtractOperation, Left,
                                                     Right>
[[nodiscard]] auto MakeSubtract(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<SubtractOperation>(
      std::forward<Left>(left), std::forward<Right>(right));
}

template <typename Left, typename Right>
  requires internal_expression::ExpressionOperand<Left> &&
           internal_expression::ExpressionOperand<Right> &&
           internal_expression::ValidBinaryOperation<MultiplyOperation, Left,
                                                     Right>
[[nodiscard]] auto MakeMultiply(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<MultiplyOperation>(
      std::forward<Left>(left), std::forward<Right>(right));
}

}  // namespace asc

#endif  // ASC_EXPRESSION_EXPRESSION_H_
