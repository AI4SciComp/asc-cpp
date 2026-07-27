#ifndef ASC_SPARSE_LINALG_H_
#define ASC_SPARSE_LINALG_H_

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"
#include "asc/sparse/compressed.h"
#include "asc/sparse/export.h"

namespace asc {

template <typename Scalar>
concept SparseLinearAlgebraScalar =
    std::same_as<Scalar, float> || std::same_as<Scalar, double>;

namespace internal_sparse_linalg {

using ReadFloat = float (*)(const void*, index_t);
using WriteFloat = void (*)(void*, index_t, float);
using ReadDouble = double (*)(const void*, index_t);
using WriteDouble = void (*)(void*, index_t, double);

ASC_SPARSE_EXPORT void ReferenceSpmv(float alpha, CsrView<const float> matrix,
                                     const void* input, ReadFloat read_input,
                                     float beta, void* output,
                                     ReadFloat read_output,
                                     WriteFloat write_output);
ASC_SPARSE_EXPORT void ReferenceSpmv(double alpha, CsrView<const double> matrix,
                                     const void* input, ReadDouble read_input,
                                     double beta, void* output,
                                     ReadDouble read_output,
                                     WriteDouble write_output);

template <typename Expression, SparseLinearAlgebraScalar Scalar>
Scalar ReadRankOne(const void* object, index_t index) {
  const auto& expression = *static_cast<const Expression*>(object);
  const std::array<index_t, 1> coordinate{index};
  return ReadExpression(expression, coordinate);
}

template <typename Expression, SparseLinearAlgebraScalar Scalar>
void WriteRankOne(void* object, index_t index, Scalar value) {
  auto& expression = *static_cast<Expression*>(object);
  const std::array<index_t, 1> coordinate{index};
  WriteExpression(expression, coordinate, value);
}

template <typename Output>
Status ValidateTotalWritable(const Output&) {
  // External writable adapters promise that every in-shape coordinate is a
  // unique logical destination.
  return Status::Ok();
}

template <SparseElement Element>
Status ValidateTotalWritable(CoordinateView<Element, 1> output) {
  if (output.nnz() != output.shape()[0]) {
    return Status(
        ErrorCode::kInvalidArgument,
        "A coordinate SpMV output must store every logical vector element");
  }
  for (nnz_t position = 0; position < output.nnz(); ++position) {
    const auto coordinate = internal_sparse_coordinate::CoordinateAt<1>(
        output.coordinate_data(), position);
    if (coordinate[0] != position) {
      return Status(
          ErrorCode::kInvalidArgument,
          "A coordinate SpMV output must store every logical vector element");
    }
  }
  return Status::Ok();
}

template <SparseElement MatrixElement, typename Output>
bool MatrixMayOverlapOutput(CsrView<MatrixElement> matrix,
                            const Output& output) noexcept {
  return MayAlias(matrix, WritableExpressionAlias(output));
}

template <SparseElement MatrixElement, SparseElement OutputElement>
bool MatrixMayOverlapOutput(
    CsrView<MatrixElement> matrix,
    const CoordinateView<OutputElement, 1>& output) noexcept {
  return internal_sparse_coordinate::ByteSpansOverlap(
      matrix.value_data(),
      static_cast<std::size_t>(matrix.nnz()) *
          sizeof(typename CsrView<MatrixElement>::value_type),
      output.value_data(),
      static_cast<std::size_t>(output.nnz()) *
          sizeof(typename CoordinateView<OutputElement, 1>::value_type));
}

template <typename Input, typename Output>
bool InputMayOverlapOutput(const Input& input, const Output& output) noexcept {
  return MayAlias(input, WritableExpressionAlias(output));
}

template <SparseElement InputElement, SparseElement OutputElement>
bool InputMayOverlapOutput(
    const CoordinateView<InputElement, 1>& input,
    const CoordinateView<OutputElement, 1>& output) noexcept {
  return internal_sparse_coordinate::ByteSpansOverlap(
      input.value_data(),
      static_cast<std::size_t>(input.nnz()) *
          sizeof(typename CoordinateView<InputElement, 1>::value_type),
      output.value_data(),
      static_cast<std::size_t>(output.nnz()) *
          sizeof(typename CoordinateView<OutputElement, 1>::value_type));
}

template <SparseElement MatrixElement, PlacedReadableExpression Input,
          WritableExpression Output, SparseLinearAlgebraScalar Scalar>
Status ValidateSpmv(const ExecutionContext& context,
                    CsrView<MatrixElement> matrix, const Input& input,
                    const Output& output) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Milestone 4 SpMV requires serial execution");
  }
  if (matrix.space() != MemorySpace::kHost ||
      ExpressionSpace(input) != MemorySpace::kHost ||
      ExpressionSpace(output) != MemorySpace::kHost) {
    return Status(ErrorCode::kMemoryAccess,
                  "Milestone 4 SpMV requires host memory");
  }
  if (ExpressionShape(input)[0] != matrix.shape()[1] ||
      ExpressionShape(output)[0] != matrix.shape()[0]) {
    return Status(ErrorCode::kShape, "SpMV operand shapes are incompatible");
  }
  if (WritableExpressionShape(output) != ExpressionShape(output)) {
    return Status(
        ErrorCode::kInvalidArgument,
        "A writable expression reports inconsistent readable/writable shapes");
  }
  const Status coverage_status = ValidateTotalWritable(output);
  if (!coverage_status.ok()) {
    return coverage_status;
  }
  if (MatrixMayOverlapOutput(matrix, output) ||
      InputMayOverlapOutput(input, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "SpMV output must not overlap the matrix or input");
  }
  return Status::Ok();
}

}  // namespace internal_sparse_linalg

// Computes y = alpha * A * x + beta * y in deterministic CSR row order.
// Successful execution allocates no storage or workspace and performs no
// packing, conversion, densification, transfer, synchronization, provider
// dispatch, or fallback. When beta is zero, prior output values are not read.
template <SparseElement MatrixElement, PlacedReadableExpression Input,
          typename Output, SparseLinearAlgebraScalar Scalar>
  requires std::same_as<std::remove_const_t<MatrixElement>, Scalar> &&
           WritableExpression<Output> &&
           std::same_as<ExpressionValue<Input>, Scalar> &&
           std::same_as<ExpressionValue<Output>, Scalar> &&
           (kExpressionRank<Input> == 1) && (kExpressionRank<Output> == 1)
Status Spmv(const ExecutionContext& context, Scalar alpha,
            CsrView<MatrixElement> matrix, const Input& input, Scalar beta,
            Output&& output) {
  using InputType = std::remove_cvref_t<Input>;
  using OutputType = std::remove_reference_t<Output>;
  const Status validation =
      internal_sparse_linalg::ValidateSpmv<MatrixElement, InputType, OutputType,
                                           Scalar>(context, matrix, input,
                                                   output);
  if (!validation.ok()) {
    return validation;
  }
  if constexpr (std::same_as<Scalar, float>) {
    internal_sparse_linalg::ReferenceSpmv(
        alpha, CsrView<const float>(matrix), std::addressof(input),
        &internal_sparse_linalg::ReadRankOne<InputType, float>, beta,
        std::addressof(output),
        &internal_sparse_linalg::ReadRankOne<OutputType, float>,
        &internal_sparse_linalg::WriteRankOne<OutputType, float>);
  } else {
    internal_sparse_linalg::ReferenceSpmv(
        alpha, CsrView<const double>(matrix), std::addressof(input),
        &internal_sparse_linalg::ReadRankOne<InputType, double>, beta,
        std::addressof(output),
        &internal_sparse_linalg::ReadRankOne<OutputType, double>,
        &internal_sparse_linalg::WriteRankOne<OutputType, double>);
  }
  return Status::Ok();
}

}  // namespace asc

#endif  // ASC_SPARSE_LINALG_H_
