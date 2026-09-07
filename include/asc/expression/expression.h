#ifndef ASC_EXPRESSION_EXPRESSION_H_
#define ASC_EXPRESSION_EXPRESSION_H_

/**
 * @file
 * @brief Public Expression declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_expression
 */

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

/**
 * @brief Identifies storage for conservative expression-alias checks.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
class AliasToken {
 public:
  /**
   * @brief Constructs a AliasToken with the documented ownership and validity
   * state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] identity The identity value required by this contract.
   * @ingroup asc_expression
   */
  explicit constexpr AliasToken(const void* identity) noexcept
      : identity_(identity) {}

  /**
   * @brief Returns the object's identity contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  [[nodiscard]] constexpr const void* identity() const noexcept {
    return identity_;
  }

 private:
  const void* identity_;
};

/**
 * @brief Selects the public SparsityEffect policy.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
// Preserve the established public enum representation.
// NOLINTNEXTLINE(performance-enum-size)
enum class SparsityEffect {
  kStructurePreserving,    ///< Selects structure preserving behavior.
  kStructureFiltering,     ///< Selects structure filtering behavior.
  kStructureUnion,         ///< Selects structure union behavior.
  kStructureIntersection,  ///< Selects structure intersection behavior.
  kValueDependent,         ///< Selects value dependent behavior.
  kDensifying,             ///< Selects densifying behavior.
  kDestinationRequired,    ///< Selects destination required behavior.
};

/**
 * @brief Selects the public ExpressionOperation policy.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
// Preserve the established public enum representation.
// NOLINTNEXTLINE(performance-enum-size)
enum class ExpressionOperation {
  kExternal,  ///< Selects external behavior.
  kScalar,    ///< Selects scalar behavior.
  kNegate,    ///< Selects negate behavior.
  kAdd,       ///< Selects add behavior.
  kSubtract,  ///< Selects subtract behavior.
  kMultiply,  ///< Selects multiply behavior.
  kTerminal,  ///< Selects terminal behavior.
};

// Specialize this class for an external readable expression. Specializations
// provide value_type, rank, sparsity_effect, Shape, Read, and MayAlias.
/**
 * @brief Customizes expression value, shape, access, and alias semantics.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename T>
struct ExpressionAdapter;

/**
 * @brief Customizes expression value, shape, access, and alias semantics.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename T>
  requires std::is_arithmetic_v<T>
struct ExpressionAdapter<T> {
  /**
   * @brief Defines the public value_type type used by this Expression contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  using value_type = T;
  /**
   * @brief Stores the rank value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr rank_t rank = 0;
  /**
   * @brief Stores the sparsity effect value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kValueDependent;
  /**
   * @brief Stores the operation value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr ExpressionOperation operation = ExpressionOperation::kScalar;

  /**
   * @brief Returns the object's Shape contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr std::array<extent_t, 0> Shape(const T& /*value*/) noexcept {
    return {};
  }

  /**
   * @brief Performs the public Read operation defined by the Expression
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] value Value read or written by the operation.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr T Read(const T& value,
                          std::span<const index_t, 0> /*indices*/) noexcept {
    return value;
  }

  /**
   * @brief Reports whether the documented MayAlias condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr bool MayAlias(const T& /*value*/,
                                 AliasToken /*token*/) noexcept {
    return false;
  }
};

/**
 * @brief Defines the public ReadableExpression concept contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
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

/**
 * @brief Defines the public ExpressionValue type used by this Expression
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
using ExpressionValue =
    typename ExpressionAdapter<std::remove_cvref_t<T>>::value_type;

/**
 * @brief Stores the ExpressionRank value for this contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
inline constexpr rank_t kExpressionRank =
    ExpressionAdapter<std::remove_cvref_t<T>>::rank;

/**
 * @brief Performs the public ExpressionShape operation defined by the
 * Expression contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] expression The expression value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
[[nodiscard]] constexpr auto ExpressionShape(const T& expression) {
  return ExpressionAdapter<std::remove_cvref_t<T>>::Shape(expression);
}

/**
 * @brief Performs the public ExpressionRead operation defined by the Expression
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] expression The expression value required by this contract.
 * @param[in] indices The indices value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
[[nodiscard]] constexpr ExpressionValue<T> ExpressionRead(
    const T& expression, std::span<const index_t, kExpressionRank<T>> indices) {
  return ExpressionAdapter<std::remove_cvref_t<T>>::Read(expression, indices);
}

/**
 * @brief Reports whether the documented MayAlias condition holds.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] expression The expression value required by this contract.
 * @param[in] token The token value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
[[nodiscard]] constexpr bool MayAlias(const T& expression, AliasToken token) {
  return ExpressionAdapter<std::remove_cvref_t<T>>::MayAlias(expression, token);
}

// Adapters whose reads depend on an execution context may optionally provide
// `static Status ValidateAccess(const T&, const ExecutionContext&)`. Adapters
// without the hook remain context-independent.
/**
 * @brief Validates the documented shape, access, ownership, and provider
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] expression The expression value required by this contract.
 * @return OK on success; otherwise a stable failure category with optional
 * diagnostics.
 * @ingroup asc_expression
 */
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

/**
 * @brief Performs the public ExpressionSparsityEffect operation defined by the
 * Expression contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
[[nodiscard]] constexpr SparsityEffect ExpressionSparsityEffect(
    const T& /*expression*/) noexcept {
  return ExpressionAdapter<std::remove_cvref_t<T>>::sparsity_effect;
}

/**
 * @brief Performs the public ExpressionOperationCategory operation defined by
 * the Expression contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression T>
[[nodiscard]] constexpr ExpressionOperation ExpressionOperationCategory(
    const T& /*expression*/) noexcept {
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

/**
 * @brief Defines the public NegateNode struct contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename OperandStorage>
struct ExpressionAdapter<internal_expression::NegateNode<OperandStorage>> {
 private:
  using Node = internal_expression::NegateNode<OperandStorage>;
  using Operand =
      std::remove_cvref_t<decltype(internal_expression::StoredExpression(
          std::declval<const OperandStorage&>()))>;

 public:
  /**
   * @brief Defines the public value_type type used by this Expression contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  using value_type = internal_expression::NegatedValue<Operand>;
  /**
   * @brief Stores the rank value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr rank_t rank = kExpressionRank<Operand>;
  /**
   * @brief Stores the sparsity effect value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;
  /**
   * @brief Stores the operation value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr ExpressionOperation operation = ExpressionOperation::kNegate;

  /**
   * @brief Returns the object's Shape contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr auto Shape(const Node& node) {
    return ExpressionShape(
        internal_expression::StoredExpression(node.operand_storage()));
  }

  /**
   * @brief Performs the public Read operation defined by the Expression
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @param[in] indices The indices value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr value_type Read(const Node& node,
                                   std::span<const index_t, rank> indices) {
    return -internal_expression::ReadOperand(
        internal_expression::StoredExpression(node.operand_storage()), indices);
  }

  /**
   * @brief Reports whether the documented MayAlias condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @param[in] token The token value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr bool MayAlias(const Node& node, AliasToken token) {
    return asc::MayAlias(
        internal_expression::StoredExpression(node.operand_storage()), token);
  }

  /**
   * @brief Validates the documented shape, access, ownership, and provider
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @param[in] context Execution backend and accessibility/order contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_expression
   */
  static Status ValidateAccess(const Node& node,
                               const ExecutionContext& context) {
    return ValidateExpressionAccess(
        context, internal_expression::StoredExpression(node.operand_storage()));
  }
};

/**
 * @brief Defines the public BinaryNode struct contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
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
  /**
   * @brief Defines the public value_type type used by this Expression contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  using value_type =
      internal_expression::BinaryValueType<Operation, Left, Right>;
  /**
   * @brief Stores the rank value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr rank_t rank = kLeftRank == 0 ? kRightRank : kLeftRank;
  /**
   * @brief Stores the sparsity effect value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr SparsityEffect sparsity_effect = [] {
    if constexpr ((kLeftRank == 0) != (kRightRank == 0)) {
      return SparsityEffect::kValueDependent;
    } else if constexpr (Operation == ExpressionOperation::kMultiply) {
      return SparsityEffect::kStructureIntersection;
    } else {
      return SparsityEffect::kStructureUnion;
    }
  }();
  /**
   * @brief Stores the operation value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @ingroup asc_expression
   */
  static constexpr ExpressionOperation operation = Operation;

  /**
   * @brief Returns the object's Shape contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
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

  /**
   * @brief Performs the public Read operation defined by the Expression
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @param[in] indices The indices value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
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

  /**
   * @brief Reports whether the documented MayAlias condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @param[in] token The token value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  static constexpr bool MayAlias(const Node& node, AliasToken token) {
    return asc::MayAlias(
               internal_expression::StoredExpression(node.left_storage()),
               token) ||
           asc::MayAlias(
               internal_expression::StoredExpression(node.right_storage()),
               token);
  }

  /**
   * @brief Validates the documented shape, access, ownership, and provider
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] node The node value required by this contract.
   * @param[in] context Execution backend and accessibility/order contract.
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_expression
   */
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

/**
 * @brief Performs the public MakeNegate operation defined by the Expression
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam Operand Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] operand The operand value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression Operand>
// Public API spelling is fixed by the Expression module contract.
// NOLINTNEXTLINE(readability-identifier-naming)
[[nodiscard]] constexpr auto MakeNegate(Operand&& operand) {
  using Storage = internal_expression::CapturedExpression<Operand&&>;
  return internal_expression::NegateNode<Storage>(
      internal_expression::CaptureExpression(std::forward<Operand>(operand)));
}

/**
 * @brief Performs the public MakeAdd operation defined by the Expression
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam Left Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Right Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] left The left value required by this contract.
 * @param[in] right The right value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression Left, ReadableExpression Right>
// Public API spelling is fixed by the Expression module contract.
// NOLINTNEXTLINE(readability-identifier-naming)
[[nodiscard]] auto MakeAdd(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<ExpressionOperation::kAdd>(
      std::forward<Left>(left), std::forward<Right>(right));
}

/**
 * @brief Performs the public MakeSubtract operation defined by the Expression
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam Left Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Right Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] left The left value required by this contract.
 * @param[in] right The right value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression Left, ReadableExpression Right>
// Public API spelling is fixed by the Expression module contract.
// NOLINTNEXTLINE(readability-identifier-naming)
[[nodiscard]] auto MakeSubtract(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<ExpressionOperation::kSubtract>(
      std::forward<Left>(left), std::forward<Right>(right));
}

/**
 * @brief Performs the public MakeMultiply operation defined by the Expression
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam Left Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Right Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] left The left value required by this contract.
 * @param[in] right The right value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <ReadableExpression Left, ReadableExpression Right>
// Public API spelling is fixed by the Expression module contract.
// NOLINTNEXTLINE(readability-identifier-naming)
[[nodiscard]] auto MakeMultiply(Left&& left, Right&& right) {
  return internal_expression::MakeBinary<ExpressionOperation::kMultiply>(
      std::forward<Left>(left), std::forward<Right>(right));
}

}  // namespace asc

#endif  // ASC_EXPRESSION_EXPRESSION_H_
