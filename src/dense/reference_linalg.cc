// Keep the owning public header first even though the compiled source has a
// more specific filename.
// clang-format off
#include "asc/dense/linalg.h"
// clang-format on

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/view.h"

namespace asc {
namespace internal_dense_linalg {

namespace {

Status ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Milestone 3 dense algebra requires serial execution");
  }
  return Status::Ok();
}

template <DenseElement Element, std::size_t Rank>
Status ValidateHostView(const ExecutionContext& context,
                        DenseView<Element, Rank> view) {
  if (!context.CanAccess(view.space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial dense algebra requires host memory");
  }
  return Status::Ok();
}

template <DenseElement LeftElement, DenseElement RightElement, std::size_t Rank>
bool HaveSameShape(DenseView<LeftElement, Rank> left,
                   DenseView<RightElement, Rank> right) {
  return left.shape() == right.shape();
}

template <typename Scalar, std::size_t Rank>
Scalar Read(DenseView<const Scalar, Rank> view,
            const std::array<index_t, Rank>& indices) {
  extent_t offset = 0;
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    offset += indices[dimension] * view.mapping().strides()[dimension];
  }
  return view.data()[static_cast<std::size_t>(offset)];
}

template <typename Scalar, std::size_t Rank>
Scalar& Write(DenseView<Scalar, Rank> view,
              const std::array<index_t, Rank>& indices) {
  extent_t offset = 0;
  for (std::size_t dimension = 0; dimension < Rank; ++dimension) {
    offset += indices[dimension] * view.mapping().strides()[dimension];
  }
  return view.data()[static_cast<std::size_t>(offset)];
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

template <typename Scalar, std::size_t Rank>
Status CopyImplementation(const ExecutionContext& context,
                          DenseView<const Scalar, Rank> source,
                          DenseView<Scalar, Rank> destination) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status source_status = ValidateHostView(context, source);
  if (!source_status.ok()) {
    return source_status;
  }
  const Status destination_status = ValidateHostView(context, destination);
  if (!destination_status.ok()) {
    return destination_status;
  }
  if (!HaveSameShape(source, destination)) {
    return Status(ErrorCode::kShape,
                  "Copy source and destination shapes must match");
  }
  if (source.IsExactView(destination)) {
    return Status::Ok();
  }
  if (source.MayOverlap(destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Copy rejects partially overlapping operands");
  }
  ForEachLogicalIndex(destination.shape(), [&](const auto& indices) {
    Write(destination, indices) = Read(source, indices);
  });
  return Status::Ok();
}

template <typename Scalar, std::size_t Rank>
Status ScalImplementation(const ExecutionContext& context, Scalar alpha,
                          DenseView<Scalar, Rank> destination) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status destination_status = ValidateHostView(context, destination);
  if (!destination_status.ok()) {
    return destination_status;
  }
  ForEachLogicalIndex(destination.shape(), [&](const auto& indices) {
    Write(destination, indices) *= alpha;
  });
  return Status::Ok();
}

template <typename Scalar, std::size_t Rank>
Status AxpyImplementation(const ExecutionContext& context, Scalar alpha,
                          DenseView<const Scalar, Rank> source,
                          DenseView<Scalar, Rank> destination) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status source_status = ValidateHostView(context, source);
  if (!source_status.ok()) {
    return source_status;
  }
  const Status destination_status = ValidateHostView(context, destination);
  if (!destination_status.ok()) {
    return destination_status;
  }
  if (!HaveSameShape(source, destination)) {
    return Status(ErrorCode::kShape,
                  "Axpy source and destination shapes must match");
  }
  if (!source.IsExactView(destination) && source.MayOverlap(destination)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Axpy rejects partial source/destination overlap");
  }
  ForEachLogicalIndex(destination.shape(), [&](const auto& indices) {
    Write(destination, indices) =
        alpha * Read(source, indices) + Write(destination, indices);
  });
  return Status::Ok();
}

template <typename Scalar>
Result<Scalar> DotImplementation(const ExecutionContext& context,
                                 DenseView<const Scalar, 1> left,
                                 DenseView<const Scalar, 1> right) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status left_status = ValidateHostView(context, left);
  if (!left_status.ok()) {
    return left_status;
  }
  const Status right_status = ValidateHostView(context, right);
  if (!right_status.ok()) {
    return right_status;
  }
  if (!HaveSameShape(left, right)) {
    return Status(ErrorCode::kShape, "Dot operand shapes must match");
  }
  Scalar result = 0;
  ForEachLogicalIndex(left.shape(), [&](const auto& indices) {
    result += Read(left, indices) * Read(right, indices);
  });
  return result;
}

template <typename Scalar>
Result<Scalar> Nrm2Implementation(const ExecutionContext& context,
                                  DenseView<const Scalar, 1> input) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status input_status = ValidateHostView(context, input);
  if (!input_status.ok()) {
    return input_status;
  }

  Scalar scale = 0;
  Scalar scaled_sum = 1;
  bool has_nonzero = false;
  bool has_infinity = false;
  bool has_nan = false;
  ForEachLogicalIndex(input.shape(), [&](const auto& indices) {
    const Scalar absolute_value = std::abs(Read(input, indices));
    if (std::isnan(absolute_value)) {
      has_nan = true;
      return;
    }
    if (std::isinf(absolute_value)) {
      has_infinity = true;
      return;
    }
    if (absolute_value == 0) {
      return;
    }
    has_nonzero = true;
    if (scale < absolute_value) {
      const Scalar ratio = scale / absolute_value;
      scaled_sum = 1 + scaled_sum * ratio * ratio;
      scale = absolute_value;
    } else {
      const Scalar ratio = absolute_value / scale;
      scaled_sum += ratio * ratio;
    }
  });
  if (has_nan) {
    return std::numeric_limits<Scalar>::quiet_NaN();
  }
  if (has_infinity) {
    return std::numeric_limits<Scalar>::infinity();
  }
  if (!has_nonzero) {
    return static_cast<Scalar>(0);
  }
  return scale * std::sqrt(scaled_sum);
}

Status ValidateMatrixOperation(MatrixOperation operation) {
  switch (operation) {
    case MatrixOperation::kNone:
    case MatrixOperation::kTranspose:
      return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument,
                "MatrixOperation is not a recognized enumerator");
}

template <typename Scalar>
Scalar ReadMatrix(DenseView<const Scalar, 2> matrix, MatrixOperation operation,
                  index_t row, index_t column) {
  if (operation == MatrixOperation::kNone) {
    return Read(matrix, std::array<index_t, 2>{row, column});
  }
  return Read(matrix, std::array<index_t, 2>{column, row});
}

template <typename Scalar>
Status GemvImplementation(const ExecutionContext& context,
                          MatrixOperation operation, Scalar alpha,
                          DenseView<const Scalar, 2> matrix,
                          DenseView<const Scalar, 1> input, Scalar beta,
                          DenseView<Scalar, 1> output) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status operation_status = ValidateMatrixOperation(operation);
  if (!operation_status.ok()) {
    return operation_status;
  }
  const Status matrix_status = ValidateHostView(context, matrix);
  if (!matrix_status.ok()) {
    return matrix_status;
  }
  const Status input_status = ValidateHostView(context, input);
  if (!input_status.ok()) {
    return input_status;
  }
  const Status output_status = ValidateHostView(context, output);
  if (!output_status.ok()) {
    return output_status;
  }

  const extent_t row_count = operation == MatrixOperation::kNone
                                 ? matrix.shape()[0]
                                 : matrix.shape()[1];
  const extent_t column_count = operation == MatrixOperation::kNone
                                    ? matrix.shape()[1]
                                    : matrix.shape()[0];
  if (input.shape()[0] != column_count || output.shape()[0] != row_count) {
    return Status(ErrorCode::kShape, "Gemv operand shapes are incompatible");
  }
  if (output.MayOverlap(matrix) || output.MayOverlap(input)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Gemv output must not overlap an input");
  }

  for (index_t row = 0; row < row_count; ++row) {
    Scalar product = 0;
    for (index_t column = 0; column < column_count; ++column) {
      product += ReadMatrix(matrix, operation, row, column) *
                 Read(input, std::array<index_t, 1>{column});
    }
    Scalar& destination = Write(output, std::array<index_t, 1>{row});
    if (beta == 0) {
      destination = alpha * product;
    } else {
      destination = alpha * product + beta * destination;
    }
  }
  return Status::Ok();
}

template <typename Scalar>
Status GemmImplementation(const ExecutionContext& context,
                          MatrixOperation left_operation,
                          MatrixOperation right_operation, Scalar alpha,
                          DenseView<const Scalar, 2> left,
                          DenseView<const Scalar, 2> right, Scalar beta,
                          DenseView<Scalar, 2> output) {
  const Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  const Status left_operation_status = ValidateMatrixOperation(left_operation);
  if (!left_operation_status.ok()) {
    return left_operation_status;
  }
  const Status right_operation_status =
      ValidateMatrixOperation(right_operation);
  if (!right_operation_status.ok()) {
    return right_operation_status;
  }
  const Status left_status = ValidateHostView(context, left);
  if (!left_status.ok()) {
    return left_status;
  }
  const Status right_status = ValidateHostView(context, right);
  if (!right_status.ok()) {
    return right_status;
  }
  const Status output_status = ValidateHostView(context, output);
  if (!output_status.ok()) {
    return output_status;
  }

  const extent_t row_count = left_operation == MatrixOperation::kNone
                                 ? left.shape()[0]
                                 : left.shape()[1];
  const extent_t inner_count = left_operation == MatrixOperation::kNone
                                   ? left.shape()[1]
                                   : left.shape()[0];
  const extent_t right_inner_count = right_operation == MatrixOperation::kNone
                                         ? right.shape()[0]
                                         : right.shape()[1];
  const extent_t column_count = right_operation == MatrixOperation::kNone
                                    ? right.shape()[1]
                                    : right.shape()[0];
  if (inner_count != right_inner_count || output.shape()[0] != row_count ||
      output.shape()[1] != column_count) {
    return Status(ErrorCode::kShape, "Gemm operand shapes are incompatible");
  }
  if (output.MayOverlap(left) || output.MayOverlap(right)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Gemm output must not overlap an input");
  }

  for (index_t column = 0; column < column_count; ++column) {
    for (index_t row = 0; row < row_count; ++row) {
      Scalar product = 0;
      for (index_t inner = 0; inner < inner_count; ++inner) {
        product += ReadMatrix(left, left_operation, row, inner) *
                   ReadMatrix(right, right_operation, inner, column);
      }
      Scalar& destination = Write(output, std::array<index_t, 2>{row, column});
      if (beta == 0) {
        destination = alpha * product;
      } else {
        destination = alpha * product + beta * destination;
      }
    }
  }
  return Status::Ok();
}

}  // namespace

Status ReferenceCopy(const ExecutionContext& context,
                     DenseView<const float, 1> source,
                     DenseView<float, 1> destination) {
  return CopyImplementation(context, source, destination);
}

Status ReferenceCopy(const ExecutionContext& context,
                     DenseView<const float, 2> source,
                     DenseView<float, 2> destination) {
  return CopyImplementation(context, source, destination);
}

Status ReferenceCopy(const ExecutionContext& context,
                     DenseView<const double, 1> source,
                     DenseView<double, 1> destination) {
  return CopyImplementation(context, source, destination);
}

Status ReferenceCopy(const ExecutionContext& context,
                     DenseView<const double, 2> source,
                     DenseView<double, 2> destination) {
  return CopyImplementation(context, source, destination);
}

Status ReferenceScal(const ExecutionContext& context, float alpha,
                     DenseView<float, 1> destination) {
  return ScalImplementation(context, alpha, destination);
}

Status ReferenceScal(const ExecutionContext& context, float alpha,
                     DenseView<float, 2> destination) {
  return ScalImplementation(context, alpha, destination);
}

Status ReferenceScal(const ExecutionContext& context, double alpha,
                     DenseView<double, 1> destination) {
  return ScalImplementation(context, alpha, destination);
}

Status ReferenceScal(const ExecutionContext& context, double alpha,
                     DenseView<double, 2> destination) {
  return ScalImplementation(context, alpha, destination);
}

Status ReferenceAxpy(const ExecutionContext& context, float alpha,
                     DenseView<const float, 1> source,
                     DenseView<float, 1> destination) {
  return AxpyImplementation(context, alpha, source, destination);
}

Status ReferenceAxpy(const ExecutionContext& context, float alpha,
                     DenseView<const float, 2> source,
                     DenseView<float, 2> destination) {
  return AxpyImplementation(context, alpha, source, destination);
}

Status ReferenceAxpy(const ExecutionContext& context, double alpha,
                     DenseView<const double, 1> source,
                     DenseView<double, 1> destination) {
  return AxpyImplementation(context, alpha, source, destination);
}

Status ReferenceAxpy(const ExecutionContext& context, double alpha,
                     DenseView<const double, 2> source,
                     DenseView<double, 2> destination) {
  return AxpyImplementation(context, alpha, source, destination);
}

Result<float> ReferenceDot(const ExecutionContext& context,
                           DenseView<const float, 1> left,
                           DenseView<const float, 1> right) {
  return DotImplementation(context, left, right);
}

Result<double> ReferenceDot(const ExecutionContext& context,
                            DenseView<const double, 1> left,
                            DenseView<const double, 1> right) {
  return DotImplementation(context, left, right);
}

Result<float> ReferenceNrm2(const ExecutionContext& context,
                            DenseView<const float, 1> input) {
  return Nrm2Implementation(context, input);
}

Result<double> ReferenceNrm2(const ExecutionContext& context,
                             DenseView<const double, 1> input) {
  return Nrm2Implementation(context, input);
}

Status ReferenceGemv(const ExecutionContext& context, MatrixOperation operation,
                     float alpha, DenseView<const float, 2> matrix,
                     DenseView<const float, 1> input, float beta,
                     DenseView<float, 1> output) {
  return GemvImplementation(context, operation, alpha, matrix, input, beta,
                            output);
}

Status ReferenceGemv(const ExecutionContext& context, MatrixOperation operation,
                     double alpha, DenseView<const double, 2> matrix,
                     DenseView<const double, 1> input, double beta,
                     DenseView<double, 1> output) {
  return GemvImplementation(context, operation, alpha, matrix, input, beta,
                            output);
}

Status ReferenceGemm(const ExecutionContext& context,
                     MatrixOperation left_operation,
                     MatrixOperation right_operation, float alpha,
                     DenseView<const float, 2> left,
                     DenseView<const float, 2> right, float beta,
                     DenseView<float, 2> output) {
  return GemmImplementation(context, left_operation, right_operation, alpha,
                            left, right, beta, output);
}

Status ReferenceGemm(const ExecutionContext& context,
                     MatrixOperation left_operation,
                     MatrixOperation right_operation, double alpha,
                     DenseView<const double, 2> left,
                     DenseView<const double, 2> right, double beta,
                     DenseView<double, 2> output) {
  return GemmImplementation(context, left_operation, right_operation, alpha,
                            left, right, beta, output);
}

}  // namespace internal_dense_linalg
}  // namespace asc
