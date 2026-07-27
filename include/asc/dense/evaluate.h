#ifndef ASC_DENSE_EVALUATE_H_
#define ASC_DENSE_EVALUATE_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
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

inline Status ValidateSerialContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Milestone 3 dense evaluation requires serial execution");
  }
  return Status::Ok();
}

template <std::size_t Rank>
Result<extent_t> ValidateShapeAndSize(const std::array<extent_t, Rank>& shape) {
  bool has_zero_extent = false;
  for (extent_t extent : shape) {
    if (extent < 0) {
      return Status(ErrorCode::kShape,
                    "A dense expression extent cannot be negative");
    }
    has_zero_extent = has_zero_extent || extent == 0;
  }
  if (has_zero_extent) {
    return static_cast<extent_t>(0);
  }
  extent_t size = 1;
  for (extent_t extent : shape) {
    auto product = CheckedMultiply(size, extent);
    if (!product.ok()) {
      return Status(ErrorCode::kOverflow,
                    "A dense expression logical size exceeds extent_t");
    }
    size = *product;
  }
  return size;
}

template <ReadableExpression Expression>
Status ValidateReadableStorage(const Expression&) {
  // The storage-neutral protocol defines external Read operations as directly
  // readable. Built-in dense terminals are checked more specifically below.
  return Status::Ok();
}

template <DenseElement Element, std::size_t Rank>
Status ValidateReadableStorage(const DenseView<Element, Rank>& view) {
  if (view.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial dense evaluation can read only host memory");
  }
  return Status::Ok();
}

template <typename Operation, typename Operand>
Status ValidateReadableStorage(
    const UnaryExpression<Operation, Operand>& expression) {
  return ValidateReadableStorage(expression.operand().get());
}

template <typename Operation, typename Left, typename Right>
Status ValidateReadableStorage(
    const BinaryExpression<Operation, Left, Right>& expression) {
  const Status left_status = ValidateReadableStorage(expression.left().get());
  if (!left_status.ok()) {
    return left_status;
  }
  return ValidateReadableStorage(expression.right().get());
}

template <typename T>
struct IsDenseView : std::false_type {};

template <DenseElement Element, std::size_t Rank>
struct IsDenseView<DenseView<Element, Rank>> : std::true_type {};

template <typename T>
inline constexpr bool kIsDenseView = IsDenseView<std::remove_cvref_t<T>>::value;

template <ReadableExpression Expression, DenseElement Element, std::size_t Rank>
bool ExpressionMayOverlap(const Expression& expression,
                          DenseView<Element, Rank> destination) {
  return MayAlias(expression, destination.alias_token());
}

template <DenseElement SourceElement, std::size_t SourceRank,
          DenseElement DestinationElement, std::size_t DestinationRank>
bool ExpressionMayOverlap(
    DenseView<SourceElement, SourceRank> source,
    DenseView<DestinationElement, DestinationRank> destination) {
  return source.MayOverlap(destination);
}

template <typename Operation, typename Operand, DenseElement Element,
          std::size_t Rank>
bool ExpressionMayOverlap(const UnaryExpression<Operation, Operand>& expression,
                          DenseView<Element, Rank> destination) {
  return ExpressionMayOverlap(expression.operand().get(), destination);
}

template <typename Operation, typename Left, typename Right,
          DenseElement Element, std::size_t Rank>
bool ExpressionMayOverlap(
    const BinaryExpression<Operation, Left, Right>& expression,
    DenseView<Element, Rank> destination) {
  return ExpressionMayOverlap(expression.left().get(), destination) ||
         ExpressionMayOverlap(expression.right().get(), destination);
}

template <std::size_t Rank, typename Function>
void ForEachLogicalIndex(const std::array<extent_t, Rank>& shape,
                         Function function) {
  if constexpr (Rank == 0) {
    function(std::array<index_t, 0>{});
    return;
  }

  for (extent_t extent : shape) {
    if (extent == 0) {
      return;
    }
  }

  std::array<index_t, Rank> indices{};
  while (true) {
    function(indices);
    std::size_t dimension = 0;
    for (; dimension < Rank; ++dimension) {
      ++indices[dimension];
      if (indices[dimension] < shape[dimension]) {
        break;
      }
      indices[dimension] = 0;
    }
    if (dimension == Rank) {
      break;
    }
  }
}

template <ReadableExpression Expression>
Status ValidateReduction(const ExecutionContext& context,
                         const Expression& expression) {
  const Status context_status = ValidateSerialContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const auto shape = ExpressionShape(expression);
  auto size = ValidateShapeAndSize(shape);
  if (!size.ok()) {
    return size.status();
  }
  return ValidateReadableStorage(expression);
}

template <typename Value>
Result<Value> AddForReduction(Value left, Value right) {
  if constexpr (std::integral<Value>) {
    return CheckedAdd(left, right);
  } else {
    return static_cast<Value>(left + right);
  }
}

}  // namespace internal_dense_evaluate

// Evaluates in deterministic dimension-zero-fastest order and accepts only an
// exact result value type in M3. A successful computational path creates no
// storage, workspace, or temporary. A failed validation may allocate storage
// for its Status diagnostic.
template <DenseElement Element, std::size_t Rank, ReadableExpression Expression>
  requires(!std::is_const_v<Element>) &&
          std::same_as<Element, ExpressionValue<Expression>>
Status Evaluate(const ExecutionContext& context,
                DenseView<Element, Rank> destination,
                const Expression& expression) {
  const Status context_status =
      internal_dense_evaluate::ValidateSerialContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!context.CanAccess(destination.space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial dense evaluation requires a host destination");
  }

  if constexpr (kExpressionRank<Expression> != 0 &&
                kExpressionRank<Expression> != Rank) {
    return Status(ErrorCode::kShape,
                  "Dense evaluation source and destination ranks differ");
  } else {
    const auto expression_shape = ExpressionShape(expression);
    auto source_size =
        internal_dense_evaluate::ValidateShapeAndSize(expression_shape);
    if (!source_size.ok()) {
      return source_size.status();
    }
    if constexpr (kExpressionRank<Expression> != 0) {
      if (expression_shape != destination.shape()) {
        return Status(
            ErrorCode::kShape,
            "Dense evaluation requires an exact destination shape match");
      }
    }

    const Status source_status =
        internal_dense_evaluate::ValidateReadableStorage(expression);
    if (!source_status.ok()) {
      return source_status;
    }
    if (destination.mapping().logical_size() == 0) {
      return Status::Ok();
    }

    if constexpr (internal_dense_evaluate::kIsDenseView<Expression>) {
      if (destination.IsExactView(expression)) {
        return Status::Ok();
      }
    }
    if (internal_dense_evaluate::ExpressionMayOverlap(expression,
                                                      destination)) {
      return Status(ErrorCode::kInvalidArgument,
                    "Dense evaluation rejects possible destination overlap");
    }

    internal_dense_evaluate::ForEachLogicalIndex(
        destination.shape(), [&](const auto& indices) {
          extent_t offset = 0;
          if constexpr (Rank > 0) {
            for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
              offset += indices[dimension] *
                        destination.mapping().strides()[dimension];
            }
          }
          if constexpr (kExpressionRank<Expression> == 0) {
            destination.data()[static_cast<std::size_t>(offset)] =
                ReadExpression(expression, std::span<const index_t, 0>());
          } else {
            destination.data()[static_cast<std::size_t>(offset)] =
                ReadExpression(expression,
                               std::span<const index_t, Rank>(indices));
          }
        });
    return Status::Ok();
  }
}

template <ReadableExpression Expression>
  requires std::is_arithmetic_v<ExpressionValue<Expression>> &&
           (!std::same_as<ExpressionValue<Expression>, bool>)
Result<ExpressionValue<Expression>> ReduceSum(const ExecutionContext& context,
                                              const Expression& expression) {
  using Value = ExpressionValue<Expression>;
  const Status validation =
      internal_dense_evaluate::ValidateReduction(context, expression);
  if (!validation.ok()) {
    return validation;
  }

  Value sum{};
  Status sum_status = Status::Ok();
  const auto shape = ExpressionShape(expression);
  internal_dense_evaluate::ForEachLogicalIndex(shape, [&](const auto& indices) {
    if (!sum_status.ok()) {
      return;
    }
    auto next = internal_dense_evaluate::AddForReduction(
        sum, ReadExpression(expression, std::span(indices)));
    if (!next.ok()) {
      sum_status = next.status();
      return;
    }
    sum = *next;
  });
  if (!sum_status.ok()) {
    return sum_status;
  }
  return sum;
}

template <ReadableExpression Expression>
  requires std::is_arithmetic_v<ExpressionValue<Expression>> &&
           (!std::same_as<ExpressionValue<Expression>, bool>)
Result<ExpressionValue<Expression>> ReduceMin(const ExecutionContext& context,
                                              const Expression& expression) {
  using Value = ExpressionValue<Expression>;
  const Status validation =
      internal_dense_evaluate::ValidateReduction(context, expression);
  if (!validation.ok()) {
    return validation;
  }
  const auto shape = ExpressionShape(expression);
  auto size = internal_dense_evaluate::ValidateShapeAndSize(shape);
  if (!size.ok()) {
    return size.status();
  }
  if (*size == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "The minimum of an empty dense expression is undefined");
  }

  bool initialized = false;
  Value minimum{};
  internal_dense_evaluate::ForEachLogicalIndex(shape, [&](const auto& indices) {
    const Value value = ReadExpression(expression, std::span(indices));
    if (!initialized || value < minimum) {
      minimum = value;
      initialized = true;
    }
  });
  return minimum;
}

template <ReadableExpression Expression>
  requires std::is_arithmetic_v<ExpressionValue<Expression>> &&
           (!std::same_as<ExpressionValue<Expression>, bool>)
Result<ExpressionValue<Expression>> ReduceMax(const ExecutionContext& context,
                                              const Expression& expression) {
  using Value = ExpressionValue<Expression>;
  const Status validation =
      internal_dense_evaluate::ValidateReduction(context, expression);
  if (!validation.ok()) {
    return validation;
  }
  const auto shape = ExpressionShape(expression);
  auto size = internal_dense_evaluate::ValidateShapeAndSize(shape);
  if (!size.ok()) {
    return size.status();
  }
  if (*size == 0) {
    return Status(ErrorCode::kInvalidArgument,
                  "The maximum of an empty dense expression is undefined");
  }

  bool initialized = false;
  Value maximum{};
  internal_dense_evaluate::ForEachLogicalIndex(shape, [&](const auto& indices) {
    const Value value = ReadExpression(expression, std::span(indices));
    if (!initialized || value > maximum) {
      maximum = value;
      initialized = true;
    }
  });
  return maximum;
}

}  // namespace asc

#endif  // ASC_DENSE_EVALUATE_H_
