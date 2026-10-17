#ifndef ASC_DENSE_EVALUATE_H_
#define ASC_DENSE_EVALUATE_H_

/**
 * @file
 * @brief Public Dense declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_dense
 */

#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"
#include "asc/expression/expression.h"

namespace asc {
namespace internal_dense_evaluate {

inline Status ValidateContext(const ExecutionContext& context,
                              MemorySpace memory_space) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Dense CPU operations require serial execution");
  }
  if (memory_space != MemorySpace::kHost || !context.CanAccess(memory_space)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Dense CPU operations require host-accessible storage");
  }
  return Status::Ok();
}

template <std::size_t Rank, typename Function>
Status ForEachCoordinate(std::span<const extent_t, Rank> extents,
                         Function&& function) {
  for (extent_t extent : extents) {
    if (extent == 0) {
      return Status::Ok();
    }
  }

  std::array<index_t, Rank> coordinates{};
  if constexpr (Rank == 0) {
    return function(std::span<const index_t, 0>(coordinates));
  } else {
    while (true) {
      Status status = function(std::span<const index_t, Rank>(coordinates));
      if (!status.ok()) {
        return status;
      }

      std::size_t dimension = 0;
      for (; dimension < Rank; ++dimension) {
        ++coordinates[dimension];
        if (coordinates[dimension] < extents[dimension]) {
          break;
        }
        coordinates[dimension] = 0;
      }
      if (dimension == Rank) {
        return Status::Ok();
      }
    }
  }
}

template <typename T>
struct IsDenseView : std::false_type {};

template <typename Element, std::size_t Rank>
struct IsDenseView<DenseView<Element, Rank>> : std::true_type {};

template <typename T>
inline constexpr bool kIsDenseView = IsDenseView<std::remove_cvref_t<T>>::value;

template <typename LeftElement, typename RightElement, std::size_t Rank>
bool SameDescriptor(const DenseView<LeftElement, Rank>& left,
                    const DenseView<RightElement, Rank>& right) {
  if (static_cast<const void*>(left.data()) !=
          static_cast<const void*>(right.data()) ||
      left.memory_space() != right.memory_space()) {
    return false;
  }
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    if (left.extents()[dimension] != right.extents()[dimension] ||
        left.strides()[dimension] != right.strides()[dimension]) {
      return false;
    }
  }
  return true;
}

template <ReadableExpression Expression, typename Element, std::size_t Rank>
Status ValidateNoAlias(const Expression& expression,
                       DenseView<Element, Rank> destination) {
  return ForEachCoordinate<Rank>(
      destination.extents(), [&](std::span<const index_t, Rank> coordinates) {
        auto destination_element = destination.At(coordinates);
        if (!destination_element.ok()) {
          return destination_element.status();
        }
        if (MayAlias(expression, AliasToken(*destination_element))) {
          return Status(
              ErrorCode::kInvalidArgument,
              "Dense evaluation rejects possible destination overlap");
        }
        return Status::Ok();
      });
}

template <typename Element>
Result<Element> AddReductionValue(Element accumulator, Element value) {
  if constexpr (std::integral<Element>) {
    return CheckedAdd(accumulator, value);
  } else {
    return static_cast<Element>(accumulator + value);
  }
}

}  // namespace internal_dense_evaluate

/**
 * @brief Performs the public Evaluate operation defined by the Dense contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Dense module contract.
 *
 * @tparam Expression Type or non-type argument satisfying the declaration's
 * constraints.
 * @tparam Rank Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] expression The expression value required by this contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @return OK on success; otherwise a stable failure category with optional
 * diagnostics.
 * @ingroup asc_dense
 */
template <ReadableExpression Expression, typename Element, std::size_t Rank>
  requires(!std::is_const_v<Element> && DenseElement<Element> &&
           std::same_as<ExpressionValue<Expression>, Element>)
Status Evaluate(const ExecutionContext& context, const Expression& expression,
                DenseView<Element, Rank> destination) {
  Status context_status = internal_dense_evaluate::ValidateContext(
      context, destination.memory_space());
  if (!context_status.ok()) {
    return context_status;
  }
  Status expression_access_status =
      ValidateExpressionAccess(context, expression);
  if (!expression_access_status.ok()) {
    return expression_access_status;
  }

  if constexpr (kExpressionRank<Expression> != 0 &&
                kExpressionRank<Expression> != Rank) {
    return Status(ErrorCode::kShape,
                  "Expression and destination ranks do not match");
  } else {
    if constexpr (kExpressionRank<Expression> == Rank) {
      const auto expression_shape = ExpressionShape(expression);
      for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
        if (expression_shape[dimension] != destination.extents()[dimension]) {
          return Status(ErrorCode::kShape,
                        "Expression and destination extents do not match");
        }
      }
    }

    if constexpr (internal_dense_evaluate::kIsDenseView<Expression>) {
      using Source = std::remove_cvref_t<Expression>;
      if constexpr (Source::kRank == Rank) {
        if (internal_dense_evaluate::SameDescriptor(expression, destination)) {
          return Status::Ok();
        }
      }
    }

    Status alias_status =
        internal_dense_evaluate::ValidateNoAlias(expression, destination);
    if (!alias_status.ok()) {
      return alias_status;
    }

    return internal_dense_evaluate::ForEachCoordinate<Rank>(
        destination.extents(), [&](std::span<const index_t, Rank> coordinates) {
          auto destination_element = destination.At(coordinates);
          if (!destination_element.ok()) {
            return destination_element.status();
          }
          if constexpr (kExpressionRank<Expression> == 0) {
            **destination_element =
                ExpressionRead(expression, std::span<const index_t, 0>());
          } else {
            **destination_element = ExpressionRead(expression, coordinates);
          }
          return Status::Ok();
        });
  }
}

/**
 * @brief Performs the public ReduceSum operation defined by the Dense contract.
 * Complex values are added algebraically without conjugation, in the existing
 * first-dimension-fastest coordinate order; empty input returns zero.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Dense module contract.
 *
 * @tparam Rank Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
  requires DenseElement<std::remove_const_t<Element>>
Result<std::remove_const_t<Element>> ReduceSum(
    const ExecutionContext& context, DenseView<Element, Rank> source) {
  using Value = std::remove_const_t<Element>;
  Status context_status =
      internal_dense_evaluate::ValidateContext(context, source.memory_space());
  if (!context_status.ok()) {
    return context_status;
  }

  Value result{};
  Status status = internal_dense_evaluate::ForEachCoordinate<Rank>(
      source.extents(), [&](std::span<const index_t, Rank> coordinates) {
        auto element = source.At(coordinates);
        if (!element.ok()) {
          return element.status();
        }
        auto next =
            internal_dense_evaluate::AddReductionValue(result, **element);
        if (!next.ok()) {
          return next.status();
        }
        result = *next;
        return Status::Ok();
      });
  if (!status.ok()) {
    return status;
  }
  return result;
}

/**
 * @brief Performs the public ReduceMin operation defined by the Dense contract.
 * This ordered reduction accepts arithmetic elements only; complex values
 * have no implicit ordering and do not satisfy this overload's constraints.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Dense module contract.
 *
 * @tparam Rank Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
  requires(DenseElement<std::remove_const_t<Element>> &&
           std::is_arithmetic_v<std::remove_const_t<Element>>)
Result<std::remove_const_t<Element>> ReduceMin(
    const ExecutionContext& context, DenseView<Element, Rank> source) {
  using Value = std::remove_const_t<Element>;
  Status context_status =
      internal_dense_evaluate::ValidateContext(context, source.memory_space());
  if (!context_status.ok()) {
    return context_status;
  }
  if (source.logical_size() == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Minimum of an empty DenseView is undefined");
  }

  Value result{};
  bool initialized = false;
  Status status = internal_dense_evaluate::ForEachCoordinate<Rank>(
      source.extents(), [&](std::span<const index_t, Rank> coordinates) {
        auto element = source.At(coordinates);
        if (!element.ok()) {
          return element.status();
        }
        if (!initialized || **element < result) {
          result = **element;
          initialized = true;
        }
        return Status::Ok();
      });
  if (!status.ok()) {
    return status;
  }
  return result;
}

/**
 * @brief Performs the public ReduceMax operation defined by the Dense contract.
 * This ordered reduction accepts arithmetic elements only; complex values
 * have no implicit ordering and do not satisfy this overload's constraints.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Dense module contract.
 *
 * @tparam Rank Type or non-type argument satisfying the declaration's
 * constraints.
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[in] source Input source, valid and accessible for the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_dense
 */
template <typename Element, std::size_t Rank>
  requires(DenseElement<std::remove_const_t<Element>> &&
           std::is_arithmetic_v<std::remove_const_t<Element>>)
Result<std::remove_const_t<Element>> ReduceMax(
    const ExecutionContext& context, DenseView<Element, Rank> source) {
  using Value = std::remove_const_t<Element>;
  Status context_status =
      internal_dense_evaluate::ValidateContext(context, source.memory_space());
  if (!context_status.ok()) {
    return context_status;
  }
  if (source.logical_size() == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "Maximum of an empty DenseView is undefined");
  }

  Value result{};
  bool initialized = false;
  Status status = internal_dense_evaluate::ForEachCoordinate<Rank>(
      source.extents(), [&](std::span<const index_t, Rank> coordinates) {
        auto element = source.At(coordinates);
        if (!element.ok()) {
          return element.status();
        }
        if (!initialized || **element > result) {
          result = **element;
          initialized = true;
        }
        return Status::Ok();
      });
  if (!status.ok()) {
    return status;
  }
  return result;
}

}  // namespace asc

#endif  // ASC_DENSE_EVALUATE_H_
