#ifndef ASC_EXPRESSION_EXPRESSION_H_
#define ASC_EXPRESSION_EXPRESSION_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <span>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc {

class AliasToken {
 public:
  explicit constexpr AliasToken(const void* identity) noexcept
      : identity_(identity) {}

  [[nodiscard]] constexpr const void* identity() const noexcept {
    return identity_;
  }

 private:
  const void* identity_;
};

enum class SparsityEffect {
  kStructurePreserving,
  kStructureFiltering,
  kStructureUnion,
  kStructureIntersection,
  kValueDependent,
  kDensifying,
  kDestinationRequired,
};

enum class ExpressionOperation {
  kExternal,
  kScalar,
  kNegate,
  kAdd,
  kSubtract,
  kMultiply,
  kTerminal,
};

// Specialize this class for an external readable expression. Specializations
// provide value_type, rank, sparsity_effect, Shape, Read, and MayAlias.
template <typename T>
struct ExpressionAdapter;

template <typename T>
  requires std::is_arithmetic_v<T>
struct ExpressionAdapter<T> {
  using value_type = T;
  static constexpr rank_t rank = 0;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kValueDependent;
  static constexpr ExpressionOperation operation = ExpressionOperation::kScalar;

  static constexpr std::array<extent_t, 0> Shape(const T&) noexcept {
    return {};
  }

  static constexpr T Read(const T& value,
                          std::span<const index_t, 0>) noexcept {
    return value;
  }

  static constexpr bool MayAlias(const T&, AliasToken) noexcept {
    return false;
  }
};

template <typename T>
concept ReadableExpression =
    requires(const std::remove_cvref_t<T>& expression, AliasToken token) {
      typename ExpressionAdapter<std::remove_cvref_t<T>>::value_type;
      {
        ExpressionAdapter<std::remove_cvref_t<T>>::rank
      } -> std::convertible_to<rank_t>;
      {
        ExpressionAdapter<std::remove_cvref_t<T>>::sparsity_effect
      } -> std::convertible_to<SparsityEffect>;
      {
        ExpressionAdapter<std::remove_cvref_t<T>>::Shape(expression)
      } -> std::same_as<std::array<
            extent_t, ExpressionAdapter<std::remove_cvref_t<T>>::rank>>;
      {
        ExpressionAdapter<std::remove_cvref_t<T>>::MayAlias(expression, token)
      } -> std::convertible_to<bool>;
    } && requires(const std::remove_cvref_t<T>& expression,
                  std::span<const index_t,
                            ExpressionAdapter<std::remove_cvref_t<T>>::rank>
                      indices) {
      {
        ExpressionAdapter<std::remove_cvref_t<T>>::Read(expression, indices)
      } -> std::convertible_to<
            typename ExpressionAdapter<std::remove_cvref_t<T>>::value_type>;
    };

template <ReadableExpression T>
using ExpressionValue =
    typename ExpressionAdapter<std::remove_cvref_t<T>>::value_type;

template <ReadableExpression T>
inline constexpr rank_t kExpressionRank =
    ExpressionAdapter<std::remove_cvref_t<T>>::rank;

template <ReadableExpression T>
[[nodiscard]] constexpr auto ExpressionShape(const T& expression) {
  return ExpressionAdapter<std::remove_cvref_t<T>>::Shape(expression);
}

template <ReadableExpression T>
[[nodiscard]] constexpr ExpressionValue<T> ExpressionRead(
    const T& expression, std::span<const index_t, kExpressionRank<T>> indices) {
  return ExpressionAdapter<std::remove_cvref_t<T>>::Read(expression, indices);
}

template <ReadableExpression T>
[[nodiscard]] constexpr bool MayAlias(const T& expression, AliasToken token) {
  return ExpressionAdapter<std::remove_cvref_t<T>>::MayAlias(expression, token);
}

// Adapters whose reads depend on an execution context may optionally provide
// `static Status ValidateAccess(const T&, const ExecutionContext&)`. Adapters
// without the hook remain context-independent.
template <ReadableExpression T>
[[nodiscard]] Status ValidateExpressionAccess(const ExecutionContext& context,
                                              const T& expression) {
  using Adapter = ExpressionAdapter<std::remove_cvref_t<T>>;
  if constexpr (requires {
                  {
                    Adapter::ValidateAccess(expression, context)
                  } -> std::same_as<Status>;
                }) {
    return Adapter::ValidateAccess(expression, context);
  } else {
    return Status::Ok();
  }
}

template <ReadableExpression T>
[[nodiscard]] constexpr SparsityEffect ExpressionSparsityEffect(
    const T&) noexcept {
  return ExpressionAdapter<std::remove_cvref_t<T>>::sparsity_effect;
}

template <ReadableExpression T>
[[nodiscard]] constexpr ExpressionOperation ExpressionOperationCategory(
    const T&) noexcept {
  using Adapter = ExpressionAdapter<std::remove_cvref_t<T>>;
  if constexpr (requires { Adapter::operation; }) {
    return Adapter::operation;
  } else {
    return ExpressionOperation::kExternal;
  }
}

namespace internal_expression {

template <typename T>
struct IsReferenceWrapper : std::false_type {};

template <typename T>
struct IsReferenceWrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
inline constexpr bool kIsReferenceWrapper =
    IsReferenceWrapper<std::remove_cv_t<T>>::value;

template <typename T>
using CapturedExpression = std::conditional_t<
    std::is_arithmetic_v<std::remove_cvref_t<T>>, std::remove_cvref_t<T>,
    std::conditional_t<std::is_lvalue_reference_v<T>,
                       std::reference_wrapper<const std::remove_reference_t<T>>,
                       std::remove_cvref_t<T>>>;

template <typename T>
constexpr CapturedExpression<T&&> CaptureExpression(T&& expression) {
  if constexpr (std::is_arithmetic_v<std::remove_cvref_t<T>>) {
    return static_cast<std::remove_cvref_t<T>>(expression);
  } else if constexpr (std::is_lvalue_reference_v<T&&>) {
    return std::cref(expression);
  } else {
    return std::forward<T>(expression);
  }
}

template <typename Storage>
constexpr const auto& StoredExpression(const Storage& storage) noexcept {
  if constexpr (kIsReferenceWrapper<Storage>) {
    return storage.get();
  } else {
    return storage;
  }
}

template <typename OperandStorage>
class NegateNode {
 public:
  explicit constexpr NegateNode(OperandStorage operand)
      : operand_(std::move(operand)) {}

  [[nodiscard]] constexpr const OperandStorage& operand_storage()
      const noexcept {
    return operand_;
  }

 private:
  OperandStorage operand_;
};

template <typename LeftStorage, typename RightStorage,
          ExpressionOperation Operation>
class BinaryNode {
 public:
  constexpr BinaryNode(LeftStorage left, RightStorage right)
      : left_(std::move(left)), right_(std::move(right)) {}

  [[nodiscard]] constexpr const LeftStorage& left_storage() const noexcept {
    return left_;
  }

  [[nodiscard]] constexpr const RightStorage& right_storage() const noexcept {
    return right_;
  }

 private:
  LeftStorage left_;
  RightStorage right_;
};

template <ReadableExpression Operand>
using NegatedValue =
    std::remove_cvref_t<decltype(-std::declval<ExpressionValue<Operand>>())>;

template <ExpressionOperation Operation, ReadableExpression Left,
          ReadableExpression Right>
struct BinaryValue;

template <ReadableExpression Left, ReadableExpression Right>
struct BinaryValue<ExpressionOperation::kAdd, Left, Right> {
  using type =
      std::remove_cvref_t<decltype(std::declval<ExpressionValue<Left>>() +
                                   std::declval<ExpressionValue<Right>>())>;
};

template <ReadableExpression Left, ReadableExpression Right>
struct BinaryValue<ExpressionOperation::kSubtract, Left, Right> {
  using type =
      std::remove_cvref_t<decltype(std::declval<ExpressionValue<Left>>() -
                                   std::declval<ExpressionValue<Right>>())>;
};

template <ReadableExpression Left, ReadableExpression Right>
struct BinaryValue<ExpressionOperation::kMultiply, Left, Right> {
  using type =
      std::remove_cvref_t<decltype(std::declval<ExpressionValue<Left>>() *
                                   std::declval<ExpressionValue<Right>>())>;
};

template <ExpressionOperation Operation, ReadableExpression Left,
          ReadableExpression Right>
using BinaryValueType = typename BinaryValue<Operation, Left, Right>::type;

template <ReadableExpression Operand, std::size_t NodeRank>
constexpr ExpressionValue<Operand> ReadOperand(
    const Operand& operand, std::span<const index_t, NodeRank> indices) {
  if constexpr (kExpressionRank<Operand> == 0) {
    return ExpressionRead(operand, std::span<const index_t, 0>());
  } else if constexpr (kExpressionRank<Operand> == NodeRank) {
    return ExpressionRead(operand, indices);
  } else {
    std::abort();
  }
}

template <ReadableExpression Left, ReadableExpression Right>
Status ValidateBinaryShape(const Left& left, const Right& right) {
  constexpr rank_t kLeftRank = kExpressionRank<Left>;
  constexpr rank_t kRightRank = kExpressionRank<Right>;
  if constexpr (kLeftRank == 0 || kRightRank == 0) {
    return Status::Ok();
  } else if constexpr (kLeftRank != kRightRank) {
    return Status(ErrorCode::kShape, "Expression ranks do not match");
  } else {
    const auto left_shape = ExpressionShape(left);
    const auto right_shape = ExpressionShape(right);
    if (left_shape != right_shape) {
      return Status(ErrorCode::kShape, "Expression extents do not match");
    }
    return Status::Ok();
  }
}

template <ExpressionOperation Operation, typename Left, typename Right>
auto MakeBinary(Left&& left, Right&& right) {
  using LeftStorage = CapturedExpression<Left&&>;
  using RightStorage = CapturedExpression<Right&&>;
  using Node = BinaryNode<LeftStorage, RightStorage, Operation>;

  const Status shape_status = ValidateBinaryShape(left, right);
  if (!shape_status.ok()) {
    return Result<Node>::Failure(shape_status);
  }
  return Result<Node>(Node(CaptureExpression(std::forward<Left>(left)),
                           CaptureExpression(std::forward<Right>(right))));
}

}  // namespace internal_expression

template <typename OperandStorage>
struct ExpressionAdapter<internal_expression::NegateNode<OperandStorage>> {
 private:
  using Node = internal_expression::NegateNode<OperandStorage>;
  using Operand =
      std::remove_cvref_t<decltype(internal_expression::StoredExpression(
          std::declval<const OperandStorage&>()))>;

 public:
  using value_type = internal_expression::NegatedValue<Operand>;
  static constexpr rank_t rank = kExpressionRank<Operand>;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  static constexpr ExpressionOperation operation = ExpressionOperation::kNegate;

  static constexpr auto Shape(const Node& node) {
    return ExpressionShape(
        internal_expression::StoredExpression(node.operand_storage()));
  }

  static constexpr value_type Read(const Node& node,
                                   std::span<const index_t, rank> indices) {
    return -internal_expression::ReadOperand(
        internal_expression::StoredExpression(node.operand_storage()), indices);
  }

  static constexpr bool MayAlias(const Node& node, AliasToken token) {
    return asc::MayAlias(
        internal_expression::StoredExpression(node.operand_storage()), token);
  }

  static Status ValidateAccess(const Node& node,
                               const ExecutionContext& context) {
    return ValidateExpressionAccess(
        context, internal_expression::StoredExpression(node.operand_storage()));
  }
};

template <typename LeftStorage, typename RightStorage,
          ExpressionOperation Operation>
struct ExpressionAdapter<
    internal_expression::BinaryNode<LeftStorage, RightStorage, Operation>> {
 private:
  using Node =
      internal_expression::BinaryNode<LeftStorage, RightStorage, Operation>;
  using Left =
      std::remove_cvref_t<decltype(internal_expression::StoredExpression(
          std::declval<const LeftStorage&>()))>;
  using Right =
      std::remove_cvref_t<decltype(internal_expression::StoredExpression(
          std::declval<const RightStorage&>()))>;
  static constexpr rank_t kLeftRank = kExpressionRank<Left>;
  static constexpr rank_t kRightRank = kExpressionRank<Right>;

 public:
  using value_type =
      internal_expression::BinaryValueType<Operation, Left, Right>;
  static constexpr rank_t rank = kLeftRank == 0 ? kRightRank : kLeftRank;
  static constexpr SparsityEffect sparsity_effect =
      (kLeftRank == 0) != (kRightRank == 0)
          ? SparsityEffect::kValueDependent
          : (Operation == ExpressionOperation::kMultiply
                 ? SparsityEffect::kStructureIntersection
                 : SparsityEffect::kStructureUnion);
  static constexpr ExpressionOperation operation = Operation;

  static constexpr std::array<extent_t, rank> Shape(const Node& node) {
    if constexpr (rank == 0) {
      return {};
    } else if constexpr (kLeftRank == rank) {
      return ExpressionShape(
          internal_expression::StoredExpression(node.left_storage()));
    } else {
      return ExpressionShape(
          internal_expression::StoredExpression(node.right_storage()));
    }
  }

  static constexpr value_type Read(const Node& node,
                                   std::span<const index_t, rank> indices) {
    const auto& left =
        internal_expression::StoredExpression(node.left_storage());
    const auto& right =
        internal_expression::StoredExpression(node.right_storage());
    if constexpr (Operation == ExpressionOperation::kAdd) {
      return internal_expression::ReadOperand(left, indices) +
             internal_expression::ReadOperand(right, indices);
    } else if constexpr (Operation == ExpressionOperation::kSubtract) {
      return internal_expression::ReadOperand(left, indices) -
             internal_expression::ReadOperand(right, indices);
    } else {
      return internal_expression::ReadOperand(left, indices) *
             internal_expression::ReadOperand(right, indices);
    }
  }

  static constexpr bool MayAlias(const Node& node, AliasToken token) {
    return asc::MayAlias(
               internal_expression::StoredExpression(node.left_storage()),
               token) ||
           asc::MayAlias(
               internal_expression::StoredExpression(node.right_storage()),
               token);
  }

  static Status ValidateAccess(const Node& node,
                               const ExecutionContext& context) {
    Status left_status = ValidateExpressionAccess(
        context, internal_expression::StoredExpression(node.left_storage()));
    if (!left_status.ok()) {
      return left_status;
    }
    return ValidateExpressionAccess(
        context, internal_expression::StoredExpression(node.right_storage()));
  }
};

template <ReadableExpression Operand>
[[nodiscard]] constexpr auto MakeNegate(Operand&& operand) {
  using Storage = internal_expression::CapturedExpression<Operand&&>;
  return internal_expression::NegateNode<Storage>(
      internal_expression::CaptureExpression(std::forward<Operand>(operand)));
}

template <ReadableExpression Left, ReadableExpression Right>
[[nodiscard]] auto MakeAdd(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<ExpressionOperation::kAdd>(
      std::forward<Left>(left), std::forward<Right>(right));
}

template <ReadableExpression Left, ReadableExpression Right>
[[nodiscard]] auto MakeSubtract(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<ExpressionOperation::kSubtract>(
      std::forward<Left>(left), std::forward<Right>(right));
}

template <ReadableExpression Left, ReadableExpression Right>
[[nodiscard]] auto MakeMultiply(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<ExpressionOperation::kMultiply>(
      std::forward<Left>(left), std::forward<Right>(right));
}

}  // namespace asc

#endif  // ASC_EXPRESSION_EXPRESSION_H_
