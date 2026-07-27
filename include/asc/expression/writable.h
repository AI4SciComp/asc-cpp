#ifndef ASC_EXPRESSION_WRITABLE_H_
#define ASC_EXPRESSION_WRITABLE_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"

namespace asc {

// External readable expression types opt in to placement reporting by
// specializing this adapter in namespace asc. A specialization supplies:
//
//   static MemorySpace Space(const T&);
template <typename T>
struct ExpressionPlacementAdapter;

// External mutable expression types opt in by specializing this adapter in
// namespace asc. A specialization supplies:
//
//   using value_type = ...;
//   static constexpr rank_t kRank = ...;
//   static std::array<extent_t, kRank> Shape(const T&);
//   static AliasToken Alias(const T&);
//   static void Write(T&, std::span<const index_t, kRank>, value_type);
//
// Write is called only after the owning algorithm has validated the complete
// operation. It must not allocate, transfer, synchronize, select a provider,
// or fail.
template <typename T>
struct WritableExpressionAdapter;

namespace internal_expression_writable {

template <typename T>
using AdaptedType = std::remove_cvref_t<T>;

template <typename T>
concept HasPlacementAdapter = requires(const AdaptedType<T>& expression) {
  {
    ExpressionPlacementAdapter<AdaptedType<T>>::Space(expression)
  } -> std::same_as<MemorySpace>;
};

template <typename T>
concept HasWritableAdapterHeader = requires {
  typename WritableExpressionAdapter<AdaptedType<T>>::value_type;
  requires std::is_object_v<
      typename WritableExpressionAdapter<AdaptedType<T>>::value_type>;
  requires std::same_as<std::remove_cv_t<decltype(WritableExpressionAdapter<
                                                  AdaptedType<T>>::kRank)>,
                        rank_t>;
};

template <typename T>
  requires HasWritableAdapterHeader<T>
inline constexpr std::size_t kAdapterRank =
    static_cast<std::size_t>(WritableExpressionAdapter<AdaptedType<T>>::kRank);

template <typename T>
concept HasWritableExpressionAdapter =
    HasWritableAdapterHeader<T> &&
    requires(
        AdaptedType<T>& expression,
        typename WritableExpressionAdapter<AdaptedType<T>>::value_type value,
        std::span<const index_t, kAdapterRank<T>> indices) {
      {
        WritableExpressionAdapter<AdaptedType<T>>::Shape(expression)
      } -> std::same_as<std::array<extent_t, kAdapterRank<T>>>;
      {
        WritableExpressionAdapter<AdaptedType<T>>::Alias(expression)
      } -> std::same_as<AliasToken>;
      {
        WritableExpressionAdapter<AdaptedType<T>>::Write(expression, indices,
                                                         value)
      } -> std::same_as<void>;
    };

}  // namespace internal_expression_writable

template <typename T>
concept PlacedReadableExpression =
    (!std::is_volatile_v<std::remove_reference_t<T>>) &&
    ReadableExpression<T> &&
    internal_expression_writable::HasPlacementAdapter<T>;

template <typename T>
concept WritableExpression =
    (!std::is_const_v<std::remove_reference_t<T>>) &&
    PlacedReadableExpression<T> &&
    internal_expression_writable::HasWritableExpressionAdapter<T> &&
    std::same_as<ExpressionValue<T>, typename WritableExpressionAdapter<
                                         std::remove_cvref_t<T>>::value_type> &&
    (kExpressionRank<T> ==
     WritableExpressionAdapter<std::remove_cvref_t<T>>::kRank);

template <PlacedReadableExpression Expression>
[[nodiscard]] MemorySpace ExpressionSpace(const Expression& expression) {
  return ExpressionPlacementAdapter<std::remove_cvref_t<Expression>>::Space(
      expression);
}

template <WritableExpression Expression>
[[nodiscard]] auto WritableExpressionShape(const Expression& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<Expression>>::Shape(
      expression);
}

template <WritableExpression Expression>
[[nodiscard]] AliasToken WritableExpressionAlias(const Expression& expression) {
  return WritableExpressionAdapter<std::remove_cvref_t<Expression>>::Alias(
      expression);
}

template <WritableExpression Expression>
void WriteExpression(
    Expression& expression,
    std::span<const index_t,
              static_cast<std::size_t>(WritableExpressionAdapter<
                                       std::remove_cvref_t<Expression>>::kRank)>
        indices,
    typename WritableExpressionAdapter<
        std::remove_cvref_t<Expression>>::value_type value) {
  WritableExpressionAdapter<std::remove_cvref_t<Expression>>::Write(
      expression, indices, value);
}

}  // namespace asc

#endif  // ASC_EXPRESSION_WRITABLE_H_
