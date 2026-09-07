#ifndef ASC_EXPRESSION_WRITABLE_H_
#define ASC_EXPRESSION_WRITABLE_H_

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
#include <cstdint>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"

namespace asc {

// Conservative, non-owning alias information. A missing byte span preserves
// the identity-token-only contract used by earlier expression adapters.
/**
 * @brief Describes expression identity and conservative byte extent.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
class ExpressionAliasMetadata {
 public:
  /**
   * @brief Constructs a ExpressionAliasMetadata with the documented ownership
   * and validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] identity The identity value required by this contract.
   * @ingroup asc_expression
   */
  // Identity-only alias metadata intentionally supports implicit construction.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr ExpressionAliasMetadata(const void* identity) noexcept
      : identity_(identity) {}

  /**
   * @brief Constructs a ExpressionAliasMetadata with the documented ownership
   * and validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @param[in] identity The identity value required by this contract.
   * @param[in] data The data value required by this contract.
   * @param[in] size The size value required by this contract.
   * @ingroup asc_expression
   */
  constexpr ExpressionAliasMetadata(const void* identity, const void* data,
                                    std::size_t size) noexcept
      : identity_(identity), data_(data), size_(size), has_byte_span_(true) {}

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

  /**
   * @brief Returns the object's data contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  [[nodiscard]] constexpr const void* data() const noexcept { return data_; }

  /**
   * @brief Returns the object's size contract value.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

  /**
   * @brief Reports whether the documented has_byte_span condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Expression module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_expression
   */
  [[nodiscard]] constexpr bool has_byte_span() const noexcept {
    return has_byte_span_;
  }

 private:
  const void* identity_ = nullptr;
  const void* data_ = nullptr;
  std::size_t size_ = 0;
  bool has_byte_span_ = false;
};

/**
 * @brief Customizes expression memory placement and alias metadata.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename T>
struct ExpressionPlacementAdapter;

/**
 * @brief Defines the public PlacedReadableExpression concept contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename T>
concept PlacedReadableExpression =
    ReadableExpression<T> &&
    requires(const std::remove_cvref_t<T>& expression) {
      {
        ExpressionPlacementAdapter<std::remove_cvref_t<T>>::Space(expression)
      } -> std::same_as<MemorySpace>;
      {
        ExpressionPlacementAdapter<std::remove_cvref_t<T>>::Alias(expression)
      } -> std::same_as<ExpressionAliasMetadata>;
    };

/**
 * @brief Performs the public ExpressionSpace operation defined by the
 * Expression contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] expression The expression value required by this contract.
 * @return The memory space declared for the expression's referenced storage.
 * @ingroup asc_expression
 */
template <PlacedReadableExpression T>
[[nodiscard]] constexpr MemorySpace ExpressionSpace(const T& expression) {
  return ExpressionPlacementAdapter<std::remove_cvref_t<T>>::Space(expression);
}

/**
 * @brief Performs the public ExpressionAlias operation defined by the
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
template <PlacedReadableExpression T>
[[nodiscard]] constexpr ExpressionAliasMetadata ExpressionAlias(
    const T& expression) {
  return ExpressionPlacementAdapter<std::remove_cvref_t<T>>::Alias(expression);
}

/**
 * @brief Customizes writable shape, alias, access, and mutation behavior.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename T>
struct WritableExpressionAdapter;

/**
 * @brief Defines the public WritableExpression concept contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 * @ingroup asc_expression
 */
template <typename T>
concept WritableExpression =
    PlacedReadableExpression<T> &&
    requires(std::remove_cvref_t<T>& expression,
             std::span<const index_t, kExpressionRank<T>> indices,
             ExpressionValue<T> value) {
      {
        WritableExpressionAdapter<std::remove_cvref_t<T>>::Shape(expression)
      } -> std::same_as<std::array<extent_t, kExpressionRank<T>>>;
      {
        WritableExpressionAdapter<std::remove_cvref_t<T>>::Alias(expression)
      } -> std::same_as<ExpressionAliasMetadata>;
      {
        WritableExpressionAdapter<std::remove_cvref_t<T>>::IsUnique(expression)
      } -> std::convertible_to<bool>;
      {
        WritableExpressionAdapter<std::remove_cvref_t<T>>::Write(expression,
                                                                 indices, value)
      } -> std::same_as<void>;
    };

/**
 * @brief Performs the public WritableExpressionShape operation defined by the
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
template <WritableExpression T>
[[nodiscard]] constexpr auto WritableExpressionShape(const T& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<T>>::Shape(expression);
}

/**
 * @brief Performs the public WritableExpressionAlias operation defined by the
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
template <WritableExpression T>
[[nodiscard]] constexpr ExpressionAliasMetadata WritableExpressionAlias(
    const T& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<T>>::Alias(expression);
}

/**
 * @brief Performs the public WritableExpressionIsUnique operation defined by
 * the Expression contract.
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
template <WritableExpression T>
[[nodiscard]] constexpr bool WritableExpressionIsUnique(const T& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<T>>::IsUnique(
      expression);
}

/**
 * @brief Performs the public WriteExpression operation defined by the
 * Expression contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam T Type or non-type argument satisfying the declaration's constraints.
 * @param[in] expression The expression value required by this contract.
 * @param[in] indices The indices value required by this contract.
 * @param[in] value Value read or written by the operation.
 * @ingroup asc_expression
 */
template <WritableExpression T>
// Public API spelling is fixed by the Expression module contract.
// NOLINTNEXTLINE(readability-identifier-naming)
constexpr void WriteExpression(
    T& expression, std::span<const index_t, kExpressionRank<T>> indices,
    ExpressionValue<T> value) {
  WritableExpressionAdapter<std::remove_cvref_t<T>>::Write(expression, indices,
                                                           value);
}

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
template <WritableExpression T>
[[nodiscard]] Status ValidateWritableExpressionAccess(
    const ExecutionContext& context, const T& expression) {
  using Adapter = WritableExpressionAdapter<std::remove_cvref_t<T>>;
  if constexpr (requires {
                  {
                    Adapter::ValidateAccess(expression, context)
                  } -> std::same_as<Status>;
                }) {
    return Adapter::ValidateAccess(expression, context);
  } else {
    return ValidateExpressionAccess(context, expression);
  }
}

namespace internal_expression_writable {

inline bool ByteSpansOverlap(ExpressionAliasMetadata left,
                             ExpressionAliasMetadata right) noexcept {
  if (!left.has_byte_span() || !right.has_byte_span() || left.size() == 0 ||
      right.size() == 0) {
    return false;
  }
  if (left.data() == nullptr || right.data() == nullptr) {
    return true;
  }
  const std::uintptr_t left_begin =
      reinterpret_cast<std::uintptr_t>(left.data());
  const std::uintptr_t right_begin =
      reinterpret_cast<std::uintptr_t>(right.data());
  if (left.size() > UINTPTR_MAX - left_begin ||
      right.size() > UINTPTR_MAX - right_begin) {
    return true;
  }
  return left_begin < right_begin + right.size() &&
         right_begin < left_begin + left.size();
}

}  // namespace internal_expression_writable

/**
 * @brief Reports whether the documented ExpressionMayOverlap condition holds.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Expression module contract.
 *
 * @tparam Source Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] source Input source, valid and accessible for the operation.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_expression
 */
template <PlacedReadableExpression Source>
[[nodiscard]] bool ExpressionMayOverlap(
    const Source& source, ExpressionAliasMetadata destination) noexcept {
  const ExpressionAliasMetadata source_alias = ExpressionAlias(source);
  if (source_alias.identity() != nullptr &&
      source_alias.identity() == destination.identity()) {
    return true;
  }
  if (internal_expression_writable::ByteSpansOverlap(source_alias,
                                                     destination)) {
    return true;
  }
  if (destination.identity() != nullptr &&
      MayAlias(source, AliasToken(destination.identity()))) {
    return true;
  }
  return false;
}

}  // namespace asc

#endif  // ASC_EXPRESSION_WRITABLE_H_
