#ifndef ASC_EXPRESSION_WRITABLE_H_
#define ASC_EXPRESSION_WRITABLE_H_

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
class ExpressionAliasMetadata {
 public:
  // Identity-only alias metadata intentionally supports implicit construction.
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr ExpressionAliasMetadata(const void* identity) noexcept
      : identity_(identity) {}

  constexpr ExpressionAliasMetadata(const void* identity, const void* data,
                                    std::size_t size) noexcept
      : identity_(identity), data_(data), size_(size), has_byte_span_(true) {}

  [[nodiscard]] constexpr const void* identity() const noexcept {
    return identity_;
  }

  [[nodiscard]] constexpr const void* data() const noexcept { return data_; }

  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

  [[nodiscard]] constexpr bool has_byte_span() const noexcept {
    return has_byte_span_;
  }

 private:
  const void* identity_ = nullptr;
  const void* data_ = nullptr;
  std::size_t size_ = 0;
  bool has_byte_span_ = false;
};

template <typename T>
struct ExpressionPlacementAdapter;

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

template <PlacedReadableExpression T>
[[nodiscard]] constexpr MemorySpace ExpressionSpace(const T& expression) {
  return ExpressionPlacementAdapter<std::remove_cvref_t<T>>::Space(expression);
}

template <PlacedReadableExpression T>
[[nodiscard]] constexpr ExpressionAliasMetadata ExpressionAlias(
    const T& expression) {
  return ExpressionPlacementAdapter<std::remove_cvref_t<T>>::Alias(expression);
}

template <typename T>
struct WritableExpressionAdapter;

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

template <WritableExpression T>
[[nodiscard]] constexpr auto WritableExpressionShape(const T& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<T>>::Shape(expression);
}

template <WritableExpression T>
[[nodiscard]] constexpr ExpressionAliasMetadata WritableExpressionAlias(
    const T& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<T>>::Alias(expression);
}

template <WritableExpression T>
[[nodiscard]] constexpr bool WritableExpressionIsUnique(const T& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<T>>::IsUnique(
      expression);
}

template <WritableExpression T>
constexpr void WriteExpression(
    T& expression, std::span<const index_t, kExpressionRank<T>> indices,
    ExpressionValue<T> value) {
  WritableExpressionAdapter<std::remove_cvref_t<T>>::Write(expression, indices,
                                                           value);
}

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
  }
  return ValidateExpressionAccess(context, expression);
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
