#ifndef ASC_SPARSE_EVALUATE_H_
#define ASC_SPARSE_EVALUATE_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"

namespace asc {
namespace internal_sparse_evaluate {

template <typename T>
struct IsCoordinateView : std::false_type {};

template <typename Element, std::size_t Rank>
struct IsCoordinateView<CoordinateView<Element, Rank>> : std::true_type {};

template <typename T>
struct IsCompressedView : std::false_type {};

template <typename Element, SparseCompressedFormat Format>
struct IsCompressedView<CompressedSparseView<Element, Format>>
    : std::true_type {};

template <ReadableExpression Expression, typename Destination>
bool IsExactSelfAssignment(const Expression& expression,
                           const Destination& destination) noexcept {
  using Source = std::remove_cvref_t<Expression>;
  if constexpr (std::same_as<Source, Destination>) {
    return expression.SameDescriptor(destination);
  } else if constexpr (IsCoordinateView<Source>::value &&
                       IsCoordinateView<Destination>::value) {
    if constexpr (Source::kRank == Destination::kRank &&
                  std::same_as<typename Source::value_type,
                               typename Destination::value_type>) {
      return expression.coordinates() == destination.coordinates() &&
             expression.values() == destination.values() &&
             expression.shape() == destination.shape() &&
             expression.nnz() == destination.nnz() &&
             expression.memory_space() == destination.memory_space();
    }
  } else if constexpr (IsCompressedView<Source>::value &&
                       IsCompressedView<Destination>::value) {
    if constexpr (Source::kFormat == Destination::kFormat &&
                  std::same_as<typename Source::value_type,
                               typename Destination::value_type>) {
      return expression.outer_offsets() == destination.outer_offsets() &&
             expression.inner_indices() == destination.inner_indices() &&
             expression.values() == destination.values() &&
             expression.shape() == destination.shape() &&
             expression.nnz() == destination.nnz() &&
             expression.memory_space() == destination.memory_space();
    }
  }
  return false;
}

template <ReadableExpression Expression, typename Destination>
Status ValidateCommon(const ExecutionContext& context,
                      const Expression& expression,
                      const Destination& destination) {
  Status context_status = internal_sparse_coordinate::ValidateSerialHost(
      context, "Sparse evaluation requires serial execution");
  if (!context_status.ok()) {
    return context_status;
  }
  if (destination.memory_space() != MemorySpace::kHost ||
      !context.CanAccess(destination.memory_space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Sparse evaluation requires a host destination");
  }
  Status source_status = ValidateExpressionAccess(context, expression);
  if (!source_status.ok()) {
    return source_status;
  }
  if constexpr (PlacedReadableExpression<Expression>) {
    if (ExpressionSpace(expression) != MemorySpace::kHost ||
        !context.CanAccess(ExpressionSpace(expression))) {
      return Status(ErrorCode::kMemoryAccess,
                    "Sparse evaluation requires a host source");
    }
  }
  if (ExpressionSparsityEffect(expression) !=
      SparsityEffect::kStructurePreserving) {
    return Status(
        ErrorCode::kUnsupported,
        "Sparse evaluation requires a structure-preserving expression");
  }
  if constexpr (kExpressionRank<Expression> != Destination::kRank) {
    return Status(ErrorCode::kShape,
                  "Sparse expression and destination ranks do not match");
  } else {
    if (ExpressionShape(expression) != destination.shape()) {
      return Status(ErrorCode::kShape,
                    "Sparse expression and destination shapes do not match");
    }
  }
  return Status::Ok();
}

template <ReadableExpression Expression, typename Destination,
          typename CoordinateAt, typename ValueAt>
Status EvaluateStored(const ExecutionContext& context,
                      const Expression& expression, Destination destination,
                      CoordinateAt&& coordinate_at, ValueAt&& value_at) {
  Status common_status = ValidateCommon(context, expression, destination);
  if (!common_status.ok()) {
    return common_status;
  }
  if (IsExactSelfAssignment(expression, destination)) {
    return Status::Ok();
  }

  for (nnz_t position = 0; position < destination.nnz(); ++position) {
    auto value = value_at(destination, position);
    if (!value.ok()) {
      return value.status();
    }
    if (MayAlias(expression, AliasToken(static_cast<const void*>(*value)))) {
      return Status(ErrorCode::kInvalidArgument,
                    "Sparse evaluation rejects destination overlap");
    }
  }
  if constexpr (PlacedReadableExpression<Expression>) {
    if (ExpressionMayOverlap(expression, destination.ValueAlias())) {
      return Status(ErrorCode::kInvalidArgument,
                    "Sparse evaluation rejects destination overlap");
    }
  }

  if constexpr (kExpressionRank<Expression> == Destination::kRank) {
    for (nnz_t position = 0; position < destination.nnz(); ++position) {
      auto coordinate = coordinate_at(destination, position);
      if (!coordinate.ok()) {
        return coordinate.status();
      }
      auto value = value_at(destination, position);
      if (!value.ok()) {
        return value.status();
      }
      **value = ExpressionRead(expression, *coordinate);
    }
  }
  return Status::Ok();
}

}  // namespace internal_sparse_evaluate

template <ReadableExpression Expression, typename Element, std::size_t Rank>
  requires(!std::is_const_v<Element> &&
           std::same_as<ExpressionValue<Expression>, Element>)
Status Evaluate(const ExecutionContext& context, const Expression& expression,
                CoordinateView<Element, Rank> destination) {
  return internal_sparse_evaluate::EvaluateStored(
      context, expression, destination,
      [](CoordinateView<Element, Rank> view, nnz_t position) {
        return view.Coordinate(position);
      },
      [](CoordinateView<Element, Rank> view, nnz_t position) {
        return view.AtStored(position);
      });
}

template <ReadableExpression Expression, typename Element,
          SparseCompressedFormat Format>
  requires(!std::is_const_v<Element> &&
           std::same_as<ExpressionValue<Expression>, Element>)
Status Evaluate(const ExecutionContext& context, const Expression& expression,
                CompressedSparseView<Element, Format> destination) {
  return internal_sparse_evaluate::EvaluateStored(
      context, expression, destination,
      [](CompressedSparseView<Element, Format> view,
         nnz_t position) -> Result<std::array<index_t, 2>> {
        const extent_t outer_extent =
            internal_sparse_compressed::OuterExtent<Format>(view.extents());
        extent_t outer = 0;
        while (outer < outer_extent) {
          auto end = view.OuterOffset(outer + 1);
          if (!end.ok()) {
            return end.status();
          }
          if (position < *end) {
            break;
          }
          ++outer;
        }
        auto inner = view.InnerIndex(position);
        if (!inner.ok()) {
          return inner.status();
        }
        return internal_sparse_compressed::Coordinate<Format>(outer, *inner);
      },
      [](CompressedSparseView<Element, Format> view, nnz_t position) {
        return view.AtStored(position);
      });
}

}  // namespace asc

#endif  // ASC_SPARSE_EVALUATE_H_
