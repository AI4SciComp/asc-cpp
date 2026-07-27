#ifndef ASC_SPARSE_EVALUATE_H_
#define ASC_SPARSE_EVALUATE_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/coordinate.h"

namespace asc {
namespace internal_sparse_evaluate {

inline Status ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Milestone 4 sparse evaluation requires serial execution");
  }
  return Status::Ok();
}

template <ReadableExpression Expression>
Status ValidateReadableStorage(const Expression& expression) {
  if constexpr (PlacedReadableExpression<Expression>) {
    if (ExpressionSpace(expression) != MemorySpace::kHost) {
      return Status(ErrorCode::kMemoryAccess,
                    "Serial sparse evaluation can read only host memory");
    }
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

template <typename Expression, typename Destination>
bool IsExactAssignment(const Expression&, const Destination&) noexcept {
  return false;
}

template <SparseElement SourceElement, SparseElement DestinationElement,
          std::size_t Rank>
bool IsExactAssignment(
    CoordinateView<SourceElement, Rank> source,
    CoordinateView<DestinationElement, Rank> destination) noexcept {
  return source.IsExactView(destination);
}

template <SparseElement SourceElement, SparseElement DestinationElement,
          SparseCompressedFormat Format>
bool IsExactAssignment(
    CompressedSparseView<SourceElement, Format> source,
    CompressedSparseView<DestinationElement, Format> destination) noexcept {
  return source.IsExactView(destination);
}

inline bool ValueSpansOverlap(const void* left, nnz_t left_count,
                              std::size_t left_size, const void* right,
                              nnz_t right_count,
                              std::size_t right_size) noexcept {
  const auto left_bytes = static_cast<std::size_t>(left_count) * left_size;
  const auto right_bytes = static_cast<std::size_t>(right_count) * right_size;
  return internal_sparse_coordinate::ByteSpansOverlap(left, left_bytes, right,
                                                      right_bytes);
}

template <ReadableExpression Expression, SparseElement Element,
          std::size_t Rank>
bool ExpressionMayOverlap(const Expression& expression,
                          CoordinateView<Element, Rank> destination) noexcept {
  return MayAlias(expression, destination.alias_token());
}

template <SparseElement SourceElement, std::size_t SourceRank,
          SparseElement DestinationElement, std::size_t DestinationRank>
bool ExpressionMayOverlap(
    CoordinateView<SourceElement, SourceRank> source,
    CoordinateView<DestinationElement, DestinationRank> destination) noexcept {
  return ValueSpansOverlap(
      source.value_data(), source.nnz(),
      sizeof(typename CoordinateView<SourceElement, SourceRank>::value_type),
      destination.value_data(), destination.nnz(),
      sizeof(typename CoordinateView<DestinationElement,
                                     DestinationRank>::value_type));
}

template <SparseElement SourceElement, SparseCompressedFormat Format,
          SparseElement DestinationElement, std::size_t DestinationRank>
bool ExpressionMayOverlap(
    CompressedSparseView<SourceElement, Format> source,
    CoordinateView<DestinationElement, DestinationRank> destination) noexcept {
  return ValueSpansOverlap(
      source.value_data(), source.nnz(),
      sizeof(typename CompressedSparseView<SourceElement, Format>::value_type),
      destination.value_data(), destination.nnz(),
      sizeof(typename CoordinateView<DestinationElement,
                                     DestinationRank>::value_type));
}

template <typename Operation, typename Operand, SparseElement Element,
          std::size_t Rank>
bool ExpressionMayOverlap(const UnaryExpression<Operation, Operand>& expression,
                          CoordinateView<Element, Rank> destination) noexcept {
  return ExpressionMayOverlap(expression.operand().get(), destination);
}

template <typename Operation, typename Left, typename Right,
          SparseElement Element, std::size_t Rank>
bool ExpressionMayOverlap(
    const BinaryExpression<Operation, Left, Right>& expression,
    CoordinateView<Element, Rank> destination) noexcept {
  return ExpressionMayOverlap(expression.left().get(), destination) ||
         ExpressionMayOverlap(expression.right().get(), destination);
}

template <ReadableExpression Expression, SparseElement Element,
          SparseCompressedFormat Format>
bool ExpressionMayOverlap(
    const Expression& expression,
    CompressedSparseView<Element, Format> destination) noexcept {
  return MayAlias(expression, destination.alias_token());
}

template <SparseElement SourceElement, std::size_t SourceRank,
          SparseElement DestinationElement,
          SparseCompressedFormat DestinationFormat>
bool ExpressionMayOverlap(
    CoordinateView<SourceElement, SourceRank> source,
    CompressedSparseView<DestinationElement, DestinationFormat>
        destination) noexcept {
  return ValueSpansOverlap(
      source.value_data(), source.nnz(),
      sizeof(typename CoordinateView<SourceElement, SourceRank>::value_type),
      destination.value_data(), destination.nnz(),
      sizeof(typename CompressedSparseView<DestinationElement,
                                           DestinationFormat>::value_type));
}

template <SparseElement SourceElement, SparseCompressedFormat SourceFormat,
          SparseElement DestinationElement,
          SparseCompressedFormat DestinationFormat>
bool ExpressionMayOverlap(
    CompressedSparseView<SourceElement, SourceFormat> source,
    CompressedSparseView<DestinationElement, DestinationFormat>
        destination) noexcept {
  return ValueSpansOverlap(
      source.value_data(), source.nnz(),
      sizeof(typename CompressedSparseView<SourceElement,
                                           SourceFormat>::value_type),
      destination.value_data(), destination.nnz(),
      sizeof(typename CompressedSparseView<DestinationElement,
                                           DestinationFormat>::value_type));
}

template <typename Operation, typename Operand, SparseElement Element,
          SparseCompressedFormat Format>
bool ExpressionMayOverlap(
    const UnaryExpression<Operation, Operand>& expression,
    CompressedSparseView<Element, Format> destination) noexcept {
  return ExpressionMayOverlap(expression.operand().get(), destination);
}

template <typename Operation, typename Left, typename Right,
          SparseElement Element, SparseCompressedFormat Format>
bool ExpressionMayOverlap(
    const BinaryExpression<Operation, Left, Right>& expression,
    CompressedSparseView<Element, Format> destination) noexcept {
  return ExpressionMayOverlap(expression.left().get(), destination) ||
         ExpressionMayOverlap(expression.right().get(), destination);
}

template <typename Destination, ReadableExpression Expression>
Status ValidateEvaluation(const ExecutionContext& context,
                          const Destination& destination,
                          const Expression& expression) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (destination.space() != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial sparse evaluation requires a host destination");
  }
  if constexpr (kExpressionSparsityEffect<Expression> !=
                SparsityEffect::kStructurePreserving) {
    return Status(
        ErrorCode::kUnsupported,
        "Milestone 4 sparse evaluation requires a structure-preserving "
        "expression");
  }
  if constexpr (kExpressionRank<Expression> !=
                std::remove_cvref_t<Destination>::rank()) {
    return Status(ErrorCode::kShape,
                  "Sparse evaluation source and destination ranks differ");
  } else {
    if (ExpressionShape(expression) != destination.shape()) {
      return Status(
          ErrorCode::kShape,
          "Sparse evaluation requires an exact destination shape match");
    }
  }
  const Status source_status = ValidateReadableStorage(expression);
  if (!source_status.ok()) {
    return source_status;
  }
  if (!IsExactAssignment(expression, destination) &&
      ExpressionMayOverlap(expression, destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Sparse evaluation rejects possible destination overlap");
  }
  return Status::Ok();
}

}  // namespace internal_sparse_evaluate

// Evaluates only at an existing canonical coordinate structure. Validation is
// complete before mutation. Successful evaluation allocates no computational
// storage or workspace and performs no conversion, densification, transfer,
// synchronization, dispatch, or fallback.
template <SparseElement Element, std::size_t Rank,
          ReadableExpression Expression>
  requires(!std::is_const_v<Element>) &&
          std::same_as<Element, ExpressionValue<Expression>>
Status Evaluate(const ExecutionContext& context,
                CoordinateView<Element, Rank> destination,
                const Expression& expression) {
  const Status validation = internal_sparse_evaluate::ValidateEvaluation(
      context, destination, expression);
  if (!validation.ok()) {
    return validation;
  }
  if constexpr (kExpressionRank<Expression> != Rank) {
    return validation;
  } else {
    if (internal_sparse_evaluate::IsExactAssignment(expression, destination)) {
      return Status::Ok();
    }
    for (nnz_t position = 0; position < destination.nnz(); ++position) {
      const auto coordinate = internal_sparse_coordinate::CoordinateAt<Rank>(
          destination.coordinate_data(), position);
      destination.value_data()[static_cast<std::size_t>(position)] =
          ReadExpression(expression, coordinate);
    }
    return Status::Ok();
  }
}

template <SparseElement Element, SparseCompressedFormat Format,
          ReadableExpression Expression>
  requires(!std::is_const_v<Element>) &&
          std::same_as<Element, ExpressionValue<Expression>>
Status Evaluate(const ExecutionContext& context,
                CompressedSparseView<Element, Format> destination,
                const Expression& expression) {
  const Status validation = internal_sparse_evaluate::ValidateEvaluation(
      context, destination, expression);
  if (!validation.ok()) {
    return validation;
  }
  if constexpr (kExpressionRank<Expression> != 2) {
    return validation;
  } else {
    if (internal_sparse_evaluate::IsExactAssignment(expression, destination)) {
      return Status::Ok();
    }

    const nnz_t* const offsets = destination.outer_offset_data();
    const index_t* const indices = destination.inner_index_data();
    const extent_t outer_extent =
        internal_sparse_compressed::OuterExtent<Format>(destination.shape());
    for (index_t outer = 0; outer < outer_extent; ++outer) {
      const nnz_t begin = offsets[static_cast<std::size_t>(outer)];
      const nnz_t end = offsets[static_cast<std::size_t>(outer + 1)];
      for (nnz_t position = begin; position < end; ++position) {
        const index_t inner = indices[static_cast<std::size_t>(position)];
        const std::array<index_t, 2> coordinate =
            Format == SparseCompressedFormat::kCsr
                ? std::array<index_t, 2>{outer, inner}
                : std::array<index_t, 2>{inner, outer};
        destination.value_data()[static_cast<std::size_t>(position)] =
            ReadExpression(expression, coordinate);
      }
    }
    return Status::Ok();
  }
}

}  // namespace asc

#endif  // ASC_SPARSE_EVALUATE_H_
