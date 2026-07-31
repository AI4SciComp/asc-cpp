#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc {
namespace internal_dense_blas {
namespace {

Status ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Dense Level 3 CPU BLAS requires serial execution");
  }
  if (!context.CanAccess(MemorySpace::kHost)) {
    return Status(ErrorCode::kMemoryAccess,
                  "Serial execution cannot access host BLAS storage");
  }
  return Status::Ok();
}

bool IsHostAccessible(MemorySpace space) {
  return space == MemorySpace::kHost || space == MemorySpace::kPinnedHost;
}

template <typename View>
Status ValidateHostStorage(const View& view) {
  if (!IsHostAccessible(view.memory_space())) {
    return Status(ErrorCode::kMemoryAccess,
                  "Dense Level 3 CPU BLAS requires host-accessible storage");
  }
  return Status::Ok();
}

bool IsValidTranspose(DenseBlasTranspose transpose) {
  return transpose == DenseBlasTranspose::kNone ||
         transpose == DenseBlasTranspose::kTranspose ||
         transpose == DenseBlasTranspose::kConjugateTranspose;
}

bool IsValidTriangle(DenseBlasTriangle triangle) {
  return triangle == DenseBlasTriangle::kUpper ||
         triangle == DenseBlasTriangle::kLower;
}

bool IsValidDiagonal(DenseBlasDiagonal diagonal) {
  return diagonal == DenseBlasDiagonal::kNonUnit ||
         diagonal == DenseBlasDiagonal::kUnit;
}

bool IsValidSide(DenseBlasSide side) {
  return side == DenseBlasSide::kLeft || side == DenseBlasSide::kRight;
}

bool Overlap(ConstMemoryView left, ConstMemoryView right) {
  if (left.size() == 0 || right.size() == 0) {
    return false;
  }
  const auto left_begin = reinterpret_cast<std::uintptr_t>(left.data());
  const auto right_begin = reinterpret_cast<std::uintptr_t>(right.data());
  if (left_begin > std::numeric_limits<std::uintptr_t>::max() - left.size() ||
      right_begin > std::numeric_limits<std::uintptr_t>::max() - right.size()) {
    return true;
  }
  return left_begin < right_begin + right.size() &&
         right_begin < left_begin + left.size();
}

template <typename Element>
Element Conjugate(Element value) {
  if constexpr (DenseBlasComplex<Element>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename Element>
Element& MatrixAt(DenseBlasMatrixView<Element> matrix, index_t row,
                  index_t column) {
  const stride_t offset = matrix.layout() == DenseBlasLayout::kColumnMajor
                              ? column * matrix.leading_dimension() + row
                              : row * matrix.leading_dimension() + column;
  return matrix.data()[offset];
}

template <typename Element>
Element OperationAt(DenseBlasMatrixView<const Element> matrix,
                    DenseBlasTranspose transpose, index_t row, index_t column) {
  Element value = transpose == DenseBlasTranspose::kNone
                      ? MatrixAt(matrix, row, column)
                      : MatrixAt(matrix, column, row);
  if (transpose == DenseBlasTranspose::kConjugateTranspose) {
    value = Conjugate(value);
  }
  return value;
}

template <typename Element>
Element StructuredAt(DenseBlasMatrixView<const Element> matrix,
                     DenseBlasTriangle triangle, index_t row, index_t column,
                     bool hermitian) {
  bool reflected = false;
  if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
      (triangle == DenseBlasTriangle::kLower && row < column)) {
    const index_t temporary = row;
    row = column;
    column = temporary;
    reflected = true;
  }
  Element value = MatrixAt(matrix, row, column);
  if constexpr (DenseBlasComplex<Element>) {
    if (hermitian && row == column) {
      return Element{value.real(), 0};
    }
  }
  return hermitian && reflected ? Conjugate(value) : value;
}

template <typename Element>
Element TriangularAt(DenseBlasMatrixView<const Element> matrix,
                     DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
                     index_t row, index_t column) {
  if (row == column && diagonal == DenseBlasDiagonal::kUnit) {
    return Element{1};
  }
  if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
      (triangle == DenseBlasTriangle::kLower && row < column)) {
    return Element{0};
  }
  return MatrixAt(matrix, row, column);
}

template <typename Element>
Element TriangularOperationAt(DenseBlasMatrixView<const Element> matrix,
                              DenseBlasTriangle triangle,
                              DenseBlasTranspose transpose,
                              DenseBlasDiagonal diagonal, index_t row,
                              index_t column) {
  Element value = transpose == DenseBlasTranspose::kNone
                      ? TriangularAt(matrix, triangle, diagonal, row, column)
                      : TriangularAt(matrix, triangle, diagonal, column, row);
  if (transpose == DenseBlasTranspose::kConjugateTranspose) {
    value = Conjugate(value);
  }
  return value;
}

template <typename... Views>
bool SameLayout(const Views&... views) {
  const DenseBlasLayout layouts[] = {views.layout()...};
  for (std::size_t index = 1; index < sizeof...(Views); ++index) {
    if (layouts[index] != layouts[0]) {
      return false;
    }
  }
  return true;
}

template <typename Element>
Status ValidateMatrices(const ExecutionContext& context,
                        DenseBlasMatrixView<const Element> left,
                        DenseBlasMatrixView<const Element> right,
                        DenseBlasMatrixView<Element> output) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  for (Status status : {ValidateHostStorage(left), ValidateHostStorage(right),
                        ValidateHostStorage(output)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (!SameLayout(left, right, output)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Level 3 matrix operands must use one common layout");
  }
  if (Overlap(left.reachable_storage(), output.reachable_storage()) ||
      Overlap(right.reachable_storage(), output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Level 3 output cannot overlap a read operand");
  }
  return Status::Ok();
}

template <typename Element>
Status GemmImpl(const ExecutionContext& context,
                DenseBlasTranspose left_transpose,
                DenseBlasTranspose right_transpose, Element alpha,
                DenseBlasMatrixView<const Element> left,
                DenseBlasMatrixView<const Element> right, Element beta,
                DenseBlasMatrixView<Element> output) {
  if (!IsValidTranspose(left_transpose) || !IsValidTranspose(right_transpose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A Level 3 Gemm transpose value is invalid");
  }
  Status status = ValidateMatrices(context, left, right, output);
  if (!status.ok()) {
    return status;
  }
  const extent_t rows = left_transpose == DenseBlasTranspose::kNone
                            ? left.rows()
                            : left.columns();
  const extent_t inner = left_transpose == DenseBlasTranspose::kNone
                             ? left.columns()
                             : left.rows();
  const extent_t right_inner = right_transpose == DenseBlasTranspose::kNone
                                   ? right.rows()
                                   : right.columns();
  const extent_t columns = right_transpose == DenseBlasTranspose::kNone
                               ? right.columns()
                               : right.rows();
  if (inner != right_inner || output.rows() != rows ||
      output.columns() != columns) {
    return Status(ErrorCode::kShape,
                  "Level 3 Gemm operand shapes do not match");
  }
  for (index_t row = 0; row < rows; ++row) {
    for (index_t column = 0; column < columns; ++column) {
      Element product{0};
      if (alpha != Element{0}) {
        for (index_t index = 0; index < inner; ++index) {
          product += OperationAt(left, left_transpose, row, index) *
                     OperationAt(right, right_transpose, index, column);
        }
      }
      Element& destination = MatrixAt(output, row, column);
      destination = alpha * product +
                    (beta == Element{0} ? Element{0} : beta * destination);
    }
  }
  return Status::Ok();
}

template <typename Element>
Status StructuredMultiply(const ExecutionContext& context, DenseBlasSide side,
                          DenseBlasTriangle triangle, Element alpha,
                          DenseBlasMatrixView<const Element> structured,
                          DenseBlasMatrixView<const Element> other,
                          Element beta, DenseBlasMatrixView<Element> output,
                          bool hermitian) {
  if (!IsValidSide(side) || !IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A structured Level 3 flag is invalid");
  }
  Status status = ValidateMatrices(context, structured, other, output);
  if (!status.ok()) {
    return status;
  }
  if (structured.rows() != structured.columns() ||
      other.rows() != output.rows() || other.columns() != output.columns()) {
    return Status(ErrorCode::kShape,
                  "Structured Level 3 operand shapes do not match");
  }
  const extent_t order =
      side == DenseBlasSide::kLeft ? output.rows() : output.columns();
  if (structured.rows() != order) {
    return Status(ErrorCode::kShape,
                  "The structured matrix order does not match its side");
  }
  for (index_t row = 0; row < output.rows(); ++row) {
    for (index_t column = 0; column < output.columns(); ++column) {
      Element product{0};
      if (alpha != Element{0}) {
        for (index_t index = 0; index < order; ++index) {
          product +=
              side == DenseBlasSide::kLeft
                  ? StructuredAt(structured, triangle, row, index, hermitian) *
                        MatrixAt(other, index, column)
                  : MatrixAt(other, row, index) *
                        StructuredAt(structured, triangle, index, column,
                                     hermitian);
        }
      }
      Element& destination = MatrixAt(output, row, column);
      destination = alpha * product +
                    (beta == Element{0} ? Element{0} : beta * destination);
    }
  }
  return Status::Ok();
}

template <typename Element>
Status ValidateRankK(const ExecutionContext& context,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const Element> left,
                     DenseBlasMatrixView<const Element> right,
                     DenseBlasMatrixView<Element> output) {
  if (!IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A rank-k triangle value is invalid");
  }
  Status status = ValidateMatrices(context, left, right, output);
  if (!status.ok()) {
    return status;
  }
  if (left.rows() != right.rows() || left.columns() != right.columns()) {
    return Status(ErrorCode::kShape,
                  "Rank-k input matrices have different shapes");
  }
  if (output.rows() != output.columns()) {
    return Status(ErrorCode::kShape, "A rank-k output must be square");
  }
  return Status::Ok();
}

template <typename Element>
Status SymmetricRankK(const ExecutionContext& context,
                      DenseBlasTriangle triangle, DenseBlasTranspose transpose,
                      Element alpha, DenseBlasMatrixView<const Element> left,
                      DenseBlasMatrixView<const Element> right, Element beta,
                      DenseBlasMatrixView<Element> output, bool rank_two) {
  if (!IsValidTranspose(transpose) ||
      (DenseBlasComplex<Element> &&
       transpose == DenseBlasTranspose::kConjugateTranspose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A symmetric rank-k transpose value is invalid");
  }
  Status status = ValidateRankK(context, triangle, left, right, output);
  if (!status.ok()) {
    return status;
  }
  const extent_t order =
      transpose == DenseBlasTranspose::kNone ? left.rows() : left.columns();
  const extent_t inner =
      transpose == DenseBlasTranspose::kNone ? left.columns() : left.rows();
  if (output.rows() != order) {
    return Status(ErrorCode::kShape,
                  "A symmetric rank-k output has the wrong order");
  }
  for (index_t row = 0; row < order; ++row) {
    const index_t begin = triangle == DenseBlasTriangle::kUpper ? row : 0;
    const index_t end = triangle == DenseBlasTriangle::kUpper ? order : row + 1;
    for (index_t column = begin; column < end; ++column) {
      Element product{0};
      if (alpha != Element{0}) {
        for (index_t index = 0; index < inner; ++index) {
          product += OperationAt(left, transpose, row, index) *
                     OperationAt(right, transpose, column, index);
          if (rank_two) {
            product += OperationAt(right, transpose, row, index) *
                       OperationAt(left, transpose, column, index);
          }
        }
      }
      Element& destination = MatrixAt(output, row, column);
      destination = alpha * product +
                    (beta == Element{0} ? Element{0} : beta * destination);
    }
  }
  return Status::Ok();
}

template <DenseBlasComplex Element>
Status HermitianRankK(const ExecutionContext& context,
                      DenseBlasTriangle triangle, DenseBlasTranspose transpose,
                      Element alpha, DenseBlasMatrixView<const Element> left,
                      DenseBlasMatrixView<const Element> right,
                      DenseBlasRealType<Element> beta,
                      DenseBlasMatrixView<Element> output, bool rank_two) {
  if (transpose != DenseBlasTranspose::kNone &&
      transpose != DenseBlasTranspose::kConjugateTranspose) {
    return Status(ErrorCode::kInvalidArgument,
                  "A Hermitian rank-k transpose value is invalid");
  }
  Status status = ValidateRankK(context, triangle, left, right, output);
  if (!status.ok()) {
    return status;
  }
  const extent_t order =
      transpose == DenseBlasTranspose::kNone ? left.rows() : left.columns();
  const extent_t inner =
      transpose == DenseBlasTranspose::kNone ? left.columns() : left.rows();
  if (output.rows() != order) {
    return Status(ErrorCode::kShape,
                  "A Hermitian rank-k output has the wrong order");
  }
  for (index_t row = 0; row < order; ++row) {
    const index_t begin = triangle == DenseBlasTriangle::kUpper ? row : 0;
    const index_t end = triangle == DenseBlasTriangle::kUpper ? order : row + 1;
    for (index_t column = begin; column < end; ++column) {
      Element product{0};
      if (alpha != Element{0}) {
        for (index_t index = 0; index < inner; ++index) {
          product += alpha * OperationAt(left, transpose, row, index) *
                     Conjugate(OperationAt(right, transpose, column, index));
          if (rank_two) {
            product += Conjugate(alpha) *
                       OperationAt(right, transpose, row, index) *
                       Conjugate(OperationAt(left, transpose, column, index));
          }
        }
      }
      Element& destination = MatrixAt(output, row, column);
      const Element previous =
          row == column ? Element{destination.real(), 0} : destination;
      Element result =
          product + (beta == DenseBlasRealType<Element>{0} ? Element{0}
                                                           : beta * previous);
      if (row == column) {
        result = Element{result.real(), 0};
      }
      destination = result;
    }
  }
  return Status::Ok();
}

template <typename Element>
Status ValidateTriangular(const ExecutionContext& context, DenseBlasSide side,
                          DenseBlasTriangle triangle,
                          DenseBlasTranspose transpose,
                          DenseBlasDiagonal diagonal,
                          DenseBlasMatrixView<const Element> triangular,
                          DenseBlasMatrixView<Element> matrix) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!IsValidSide(side) || !IsValidTriangle(triangle) ||
      !IsValidTranspose(transpose) || !IsValidDiagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A triangular Level 3 flag is invalid");
  }
  for (Status status :
       {ValidateHostStorage(triangular), ValidateHostStorage(matrix)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (!SameLayout(triangular, matrix)) {
    return Status(ErrorCode::kInvalidArgument,
                  "Triangular Level 3 operands must use one common layout");
  }
  if (triangular.rows() != triangular.columns()) {
    return Status(ErrorCode::kShape, "A triangular matrix must be square");
  }
  const extent_t order =
      side == DenseBlasSide::kLeft ? matrix.rows() : matrix.columns();
  if (triangular.rows() != order) {
    return Status(ErrorCode::kShape,
                  "The triangular matrix order does not match its side");
  }
  if (Overlap(triangular.reachable_storage(), matrix.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "A triangular input cannot overlap its mutable matrix");
  }
  return Status::Ok();
}

template <typename Element>
void FillZero(DenseBlasMatrixView<Element> matrix) {
  for (index_t row = 0; row < matrix.rows(); ++row) {
    for (index_t column = 0; column < matrix.columns(); ++column) {
      MatrixAt(matrix, row, column) = Element{0};
    }
  }
}

template <typename Element>
Status TriangularMultiply(const ExecutionContext& context, DenseBlasSide side,
                          DenseBlasTriangle triangle,
                          DenseBlasTranspose transpose,
                          DenseBlasDiagonal diagonal, Element alpha,
                          DenseBlasMatrixView<const Element> triangular,
                          DenseBlasMatrixView<Element> matrix) {
  Status status = ValidateTriangular(context, side, triangle, transpose,
                                     diagonal, triangular, matrix);
  if (!status.ok()) {
    return status;
  }
  if (alpha == Element{0}) {
    FillZero(matrix);
    return Status::Ok();
  }
  const bool effective_upper = transpose == DenseBlasTranspose::kNone
                                   ? triangle == DenseBlasTriangle::kUpper
                                   : triangle == DenseBlasTriangle::kLower;
  if (side == DenseBlasSide::kLeft) {
    for (index_t column = 0; column < matrix.columns(); ++column) {
      for (index_t iteration = 0; iteration < matrix.rows(); ++iteration) {
        const index_t row =
            effective_upper ? iteration : matrix.rows() - 1 - iteration;
        const index_t begin = effective_upper ? row : 0;
        const index_t end = effective_upper ? matrix.rows() : row + 1;
        Element product{0};
        for (index_t index = begin; index < end; ++index) {
          product += TriangularOperationAt(triangular, triangle, transpose,
                                           diagonal, row, index) *
                     MatrixAt(matrix, index, column);
        }
        MatrixAt(matrix, row, column) = alpha * product;
      }
    }
    return Status::Ok();
  }
  for (index_t row = 0; row < matrix.rows(); ++row) {
    for (index_t iteration = 0; iteration < matrix.columns(); ++iteration) {
      const index_t column =
          effective_upper ? matrix.columns() - 1 - iteration : iteration;
      const index_t begin = effective_upper ? 0 : column;
      const index_t end = effective_upper ? column + 1 : matrix.columns();
      Element product{0};
      for (index_t index = begin; index < end; ++index) {
        product += MatrixAt(matrix, row, index) *
                   TriangularOperationAt(triangular, triangle, transpose,
                                         diagonal, index, column);
      }
      MatrixAt(matrix, row, column) = alpha * product;
    }
  }
  return Status::Ok();
}

template <typename Element>
Status TriangularSolve(const ExecutionContext& context, DenseBlasSide side,
                       DenseBlasTriangle triangle, DenseBlasTranspose transpose,
                       DenseBlasDiagonal diagonal, Element alpha,
                       DenseBlasMatrixView<const Element> triangular,
                       DenseBlasMatrixView<Element> matrix) {
  Status status = ValidateTriangular(context, side, triangle, transpose,
                                     diagonal, triangular, matrix);
  if (!status.ok()) {
    return status;
  }
  if (alpha == Element{0}) {
    FillZero(matrix);
    return Status::Ok();
  }
  const bool effective_upper = transpose == DenseBlasTranspose::kNone
                                   ? triangle == DenseBlasTriangle::kUpper
                                   : triangle == DenseBlasTriangle::kLower;
  if (side == DenseBlasSide::kLeft) {
    for (index_t column = 0; column < matrix.columns(); ++column) {
      for (index_t iteration = 0; iteration < matrix.rows(); ++iteration) {
        const index_t row =
            effective_upper ? matrix.rows() - 1 - iteration : iteration;
        Element result = alpha * MatrixAt(matrix, row, column);
        const index_t begin = effective_upper ? row + 1 : 0;
        const index_t end = effective_upper ? matrix.rows() : row;
        for (index_t index = begin; index < end; ++index) {
          result -= TriangularOperationAt(triangular, triangle, transpose,
                                          diagonal, row, index) *
                    MatrixAt(matrix, index, column);
        }
        if (diagonal == DenseBlasDiagonal::kNonUnit) {
          result /= TriangularOperationAt(triangular, triangle, transpose,
                                          diagonal, row, row);
        }
        MatrixAt(matrix, row, column) = result;
      }
    }
    return Status::Ok();
  }
  for (index_t row = 0; row < matrix.rows(); ++row) {
    for (index_t iteration = 0; iteration < matrix.columns(); ++iteration) {
      const index_t column =
          effective_upper ? iteration : matrix.columns() - 1 - iteration;
      Element result = alpha * MatrixAt(matrix, row, column);
      const index_t begin = effective_upper ? 0 : column + 1;
      const index_t end = effective_upper ? column : matrix.columns();
      for (index_t index = begin; index < end; ++index) {
        result -= MatrixAt(matrix, row, index) *
                  TriangularOperationAt(triangular, triangle, transpose,
                                        diagonal, index, column);
      }
      if (diagonal == DenseBlasDiagonal::kNonUnit) {
        result /= TriangularOperationAt(triangular, triangle, transpose,
                                        diagonal, column, column);
      }
      MatrixAt(matrix, row, column) = result;
    }
  }
  return Status::Ok();
}

}  // namespace
}  // namespace internal_dense_blas

template <DenseBlasScalar Element>
Status Gemm(const ExecutionContext& context, DenseBlasTranspose left_transpose,
            DenseBlasTranspose right_transpose, Element alpha,
            DenseBlasMatrixView<const Element> left,
            DenseBlasMatrixView<const Element> right, Element beta,
            DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::GemmImpl(context, left_transpose, right_transpose,
                                       alpha, left, right, beta, output);
}

template <DenseBlasScalar Element>
Status Symm(const ExecutionContext& context, DenseBlasSide side,
            DenseBlasTriangle triangle, Element alpha,
            DenseBlasMatrixView<const Element> symmetric,
            DenseBlasMatrixView<const Element> other, Element beta,
            DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::StructuredMultiply(
      context, side, triangle, alpha, symmetric, other, beta, output, false);
}

template <DenseBlasComplex Element>
Status Hemm(const ExecutionContext& context, DenseBlasSide side,
            DenseBlasTriangle triangle, Element alpha,
            DenseBlasMatrixView<const Element> hermitian,
            DenseBlasMatrixView<const Element> other, Element beta,
            DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::StructuredMultiply(
      context, side, triangle, alpha, hermitian, other, beta, output, true);
}

template <DenseBlasScalar Element>
Status Syrk(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, Element alpha,
            DenseBlasMatrixView<const Element> input, Element beta,
            DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::SymmetricRankK(
      context, triangle, transpose, alpha, input, input, beta, output, false);
}

template <DenseBlasComplex Element>
Status Herk(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasRealType<Element> alpha,
            DenseBlasMatrixView<const Element> input,
            DenseBlasRealType<Element> beta,
            DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::HermitianRankK(context, triangle, transpose,
                                             Element{alpha, 0}, input, input,
                                             beta, output, false);
}

template <DenseBlasScalar Element>
Status Syr2k(const ExecutionContext& context, DenseBlasTriangle triangle,
             DenseBlasTranspose transpose, Element alpha,
             DenseBlasMatrixView<const Element> left,
             DenseBlasMatrixView<const Element> right, Element beta,
             DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::SymmetricRankK(
      context, triangle, transpose, alpha, left, right, beta, output, true);
}

template <DenseBlasComplex Element>
Status Her2k(const ExecutionContext& context, DenseBlasTriangle triangle,
             DenseBlasTranspose transpose, Element alpha,
             DenseBlasMatrixView<const Element> left,
             DenseBlasMatrixView<const Element> right,
             DenseBlasRealType<Element> beta,
             DenseBlasMatrixView<Element> output) {
  return internal_dense_blas::HermitianRankK(
      context, triangle, transpose, alpha, left, right, beta, output, true);
}

template <DenseBlasScalar Element>
Status Trmm(const ExecutionContext& context, DenseBlasSide side,
            DenseBlasTriangle triangle, DenseBlasTranspose transpose,
            DenseBlasDiagonal diagonal, Element alpha,
            DenseBlasMatrixView<const Element> triangular,
            DenseBlasMatrixView<Element> matrix) {
  return internal_dense_blas::TriangularMultiply(
      context, side, triangle, transpose, diagonal, alpha, triangular, matrix);
}

template <DenseBlasScalar Element>
Status Trsm(const ExecutionContext& context, DenseBlasSide side,
            DenseBlasTriangle triangle, DenseBlasTranspose transpose,
            DenseBlasDiagonal diagonal, Element alpha,
            DenseBlasMatrixView<const Element> triangular,
            DenseBlasMatrixView<Element> matrix) {
  return internal_dense_blas::TriangularSolve(
      context, side, triangle, transpose, diagonal, alpha, triangular, matrix);
}

#define ASC_INSTANTIATE_LEVEL3(Type)                                          \
  template Status Gemm<Type>(                                                 \
      const ExecutionContext&, DenseBlasTranspose, DenseBlasTranspose, Type,  \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type, \
      DenseBlasMatrixView<Type>);                                             \
  template Status Symm<Type>(                                                 \
      const ExecutionContext&, DenseBlasSide, DenseBlasTriangle, Type,        \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type, \
      DenseBlasMatrixView<Type>);                                             \
  template Status Syrk<Type>(                                                 \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose, Type,   \
      DenseBlasMatrixView<const Type>, Type, DenseBlasMatrixView<Type>);      \
  template Status Syr2k<Type>(                                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose, Type,   \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type, \
      DenseBlasMatrixView<Type>);                                             \
  template Status Trmm<Type>(                                                 \
      const ExecutionContext&, DenseBlasSide, DenseBlasTriangle,              \
      DenseBlasTranspose, DenseBlasDiagonal, Type,                            \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<Type>);            \
  template Status Trsm<Type>(                                                 \
      const ExecutionContext&, DenseBlasSide, DenseBlasTriangle,              \
      DenseBlasTranspose, DenseBlasDiagonal, Type,                            \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<Type>)

#define ASC_INSTANTIATE_LEVEL3_COMPLEX(Type, Real)                            \
  ASC_INSTANTIATE_LEVEL3(Type);                                               \
  template Status Hemm<Type>(                                                 \
      const ExecutionContext&, DenseBlasSide, DenseBlasTriangle, Type,        \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Type, \
      DenseBlasMatrixView<Type>);                                             \
  template Status Herk<Type>(                                                 \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose, Real,   \
      DenseBlasMatrixView<const Type>, Real, DenseBlasMatrixView<Type>);      \
  template Status Her2k<Type>(                                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose, Type,   \
      DenseBlasMatrixView<const Type>, DenseBlasMatrixView<const Type>, Real, \
      DenseBlasMatrixView<Type>)

ASC_INSTANTIATE_LEVEL3(float);
ASC_INSTANTIATE_LEVEL3(double);
ASC_INSTANTIATE_LEVEL3_COMPLEX(std::complex<float>, float);
ASC_INSTANTIATE_LEVEL3_COMPLEX(std::complex<double>, double);

#undef ASC_INSTANTIATE_LEVEL3
#undef ASC_INSTANTIATE_LEVEL3_COMPLEX

}  // namespace asc
