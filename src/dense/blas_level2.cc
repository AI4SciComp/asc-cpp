#include <complex>
#include <cstdint>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/export.h"

namespace asc {
namespace internal_dense_blas {
namespace {

Status ValidateContext(const ExecutionContext& context) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported,
                  "Dense Level 2 CPU BLAS requires serial execution");
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
                  "Dense Level 2 CPU BLAS requires host-accessible storage");
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
Element MaybeConjugate(Element value, bool conjugate) {
  return conjugate ? Conjugate(value) : value;
}

template <typename Element>
Element& VectorAt(DenseBlasVectorView<Element> vector, index_t index) {
  return vector.data()[index * vector.increment()];
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
Element GeneralBandAt(DenseBlasBandMatrixView<const Element> matrix,
                      index_t row, index_t column) {
  if (column > row + matrix.upper_bandwidth() ||
      row > column + matrix.lower_bandwidth()) {
    return Element{0};
  }
  const stride_t offset = matrix.layout() == DenseBlasLayout::kColumnMajor
                              ? column * matrix.leading_dimension() +
                                    matrix.upper_bandwidth() + row - column
                              : row * matrix.leading_dimension() +
                                    matrix.lower_bandwidth() + column - row;
  return matrix.data()[offset];
}

template <typename Element>
Element TriangularBandStoredAt(
    DenseBlasTriangularBandView<const Element> matrix,
    DenseBlasTriangle triangle, index_t row, index_t column) {
  if (triangle == DenseBlasTriangle::kUpper) {
    if (column < row || column > row + matrix.bandwidth()) {
      return Element{0};
    }
  } else if (row < column || row > column + matrix.bandwidth()) {
    return Element{0};
  }
  stride_t offset = 0;
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    offset = triangle == DenseBlasTriangle::kUpper
                 ? column * matrix.leading_dimension() + matrix.bandwidth() +
                       row - column
                 : column * matrix.leading_dimension() + row - column;
  } else {
    offset = triangle == DenseBlasTriangle::kUpper
                 ? row * matrix.leading_dimension() + column - row
                 : row * matrix.leading_dimension() + matrix.bandwidth() +
                       column - row;
  }
  return matrix.data()[offset];
}

stride_t PackedOffset(extent_t order, DenseBlasLayout layout,
                      DenseBlasTriangle triangle, index_t row, index_t column) {
  if (layout == DenseBlasLayout::kColumnMajor) {
    if (triangle == DenseBlasTriangle::kUpper) {
      return column * (column + 1) / 2 + row;
    }
    return column * order - column * (column - 1) / 2 + row - column;
  }
  if (triangle == DenseBlasTriangle::kUpper) {
    return row * order - row * (row - 1) / 2 + column - row;
  }
  return row * (row + 1) / 2 + column;
}

template <typename Element>
Element& PackedStoredAt(DenseBlasPackedMatrixView<Element> matrix,
                        DenseBlasTriangle triangle, index_t row,
                        index_t column) {
  return matrix.data()[PackedOffset(matrix.order(), matrix.layout(), triangle,
                                    row, column)];
}

template <typename Element>
Element SymmetricAt(DenseBlasMatrixView<const Element> matrix,
                    DenseBlasTriangle triangle, index_t row, index_t column,
                    bool hermitian) {
  bool reflected = false;
  if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
      (triangle == DenseBlasTriangle::kLower && row < column)) {
    std::swap(row, column);
    reflected = true;
  }
  Element value = MatrixAt(matrix, row, column);
  if constexpr (DenseBlasComplex<Element>) {
    if (row == column && hermitian) {
      return Element{value.real(), 0};
    }
  }
  return hermitian && reflected ? Conjugate(value) : value;
}

template <typename Element>
Element SymmetricBandAt(DenseBlasTriangularBandView<const Element> matrix,
                        DenseBlasTriangle triangle, index_t row, index_t column,
                        bool hermitian) {
  if (row > column + matrix.bandwidth() || column > row + matrix.bandwidth()) {
    return Element{0};
  }
  bool reflected = false;
  if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
      (triangle == DenseBlasTriangle::kLower && row < column)) {
    std::swap(row, column);
    reflected = true;
  }
  Element value = TriangularBandStoredAt(matrix, triangle, row, column);
  if constexpr (DenseBlasComplex<Element>) {
    if (row == column && hermitian) {
      return Element{value.real(), 0};
    }
  }
  return hermitian && reflected ? Conjugate(value) : value;
}

template <typename Element>
Element SymmetricPackedAt(DenseBlasPackedMatrixView<const Element> matrix,
                          DenseBlasTriangle triangle, index_t row,
                          index_t column, bool hermitian) {
  bool reflected = false;
  if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
      (triangle == DenseBlasTriangle::kLower && row < column)) {
    std::swap(row, column);
    reflected = true;
  }
  Element value = PackedStoredAt(matrix, triangle, row, column);
  if constexpr (DenseBlasComplex<Element>) {
    if (row == column && hermitian) {
      return Element{value.real(), 0};
    }
  }
  return hermitian && reflected ? Conjugate(value) : value;
}

template <typename Matrix, typename Element, typename Access>
Status GeneralMv(const ExecutionContext& context, DenseBlasTranspose transpose,
                 Element alpha, Matrix matrix,
                 DenseBlasVectorView<const Element> input, Element beta,
                 DenseBlasVectorView<Element> output, Access access) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!IsValidTranspose(transpose)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A Level 2 transpose value is invalid");
  }
  for (Status status : {ValidateHostStorage(matrix), ValidateHostStorage(input),
                        ValidateHostStorage(output)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const extent_t output_size =
      transpose == DenseBlasTranspose::kNone ? matrix.rows() : matrix.columns();
  const extent_t input_size =
      transpose == DenseBlasTranspose::kNone ? matrix.columns() : matrix.rows();
  if (input.size() != input_size || output.size() != output_size) {
    return Status(ErrorCode::kShape,
                  "Level 2 matrix-vector operand shapes do not match");
  }
  if (Overlap(matrix.reachable_storage(), output.reachable_storage()) ||
      Overlap(input.reachable_storage(), output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Matrix-vector output cannot overlap a read operand");
  }
  for (index_t row = 0; row < output_size; ++row) {
    Element product{0};
    if (alpha != Element{0}) {
      for (index_t column = 0; column < input_size; ++column) {
        Element matrix_value =
            transpose == DenseBlasTranspose::kNone
                ? access(row, column)
                // Transpose intentionally reverses the matrix indices.
                // NOLINTNEXTLINE(readability-suspicious-call-argument)
                : access(column, row);
        matrix_value = MaybeConjugate(
            matrix_value, transpose == DenseBlasTranspose::kConjugateTranspose);
        product += matrix_value * VectorAt(input, column);
      }
    }
    Element& destination = VectorAt(output, row);
    destination = alpha * product +
                  (beta == Element{0} ? Element{0} : beta * destination);
  }
  return Status::Ok();
}

template <typename Matrix, typename Element, typename Access>
Status StructuredMv(const ExecutionContext& context, DenseBlasTriangle triangle,
                    Element alpha, Matrix matrix,
                    DenseBlasVectorView<const Element> input, Element beta,
                    DenseBlasVectorView<Element> output, Access access) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A Level 2 triangle value is invalid");
  }
  for (Status status : {ValidateHostStorage(matrix), ValidateHostStorage(input),
                        ValidateHostStorage(output)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const extent_t order = matrix.order();
  if (input.size() != order || output.size() != order) {
    return Status(ErrorCode::kShape,
                  "Structured matrix-vector operand shapes do not match");
  }
  if (Overlap(matrix.reachable_storage(), output.reachable_storage()) ||
      Overlap(input.reachable_storage(), output.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "Structured matrix-vector output overlaps a read operand");
  }
  for (index_t row = 0; row < order; ++row) {
    Element product{0};
    if (alpha != Element{0}) {
      for (index_t column = 0; column < order; ++column) {
        product += access(row, column) * VectorAt(input, column);
      }
    }
    Element& destination = VectorAt(output, row);
    destination = alpha * product +
                  (beta == Element{0} ? Element{0} : beta * destination);
  }
  return Status::Ok();
}

template <typename Element>
struct FullTriangularAccess {
  DenseBlasMatrixView<const Element> matrix;
  DenseBlasTriangle triangle;
  DenseBlasDiagonal diagonal;

  [[nodiscard]] extent_t order() const { return matrix.rows(); }
  [[nodiscard]] MemorySpace memory_space() const {
    return matrix.memory_space();
  }
  [[nodiscard]] ConstMemoryView reachable_storage() const {
    return matrix.reachable_storage();
  }
  [[nodiscard]] Element operator()(index_t row, index_t column) const {
    if (row == column && diagonal == DenseBlasDiagonal::kUnit) {
      return Element{1};
    }
    if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
        (triangle == DenseBlasTriangle::kLower && row < column)) {
      return Element{0};
    }
    return MatrixAt(matrix, row, column);
  }
};

template <typename Element>
struct BandTriangularAccess {
  DenseBlasTriangularBandView<const Element> matrix;
  DenseBlasTriangle triangle;
  DenseBlasDiagonal diagonal;

  [[nodiscard]] extent_t order() const { return matrix.order(); }
  [[nodiscard]] MemorySpace memory_space() const {
    return matrix.memory_space();
  }
  [[nodiscard]] ConstMemoryView reachable_storage() const {
    return matrix.reachable_storage();
  }
  [[nodiscard]] Element operator()(index_t row, index_t column) const {
    if (row == column && diagonal == DenseBlasDiagonal::kUnit) {
      return Element{1};
    }
    return TriangularBandStoredAt(matrix, triangle, row, column);
  }
};

template <typename Element>
struct PackedTriangularAccess {
  DenseBlasPackedMatrixView<const Element> matrix;
  DenseBlasTriangle triangle;
  DenseBlasDiagonal diagonal;

  [[nodiscard]] extent_t order() const { return matrix.order(); }
  [[nodiscard]] MemorySpace memory_space() const {
    return matrix.memory_space();
  }
  [[nodiscard]] ConstMemoryView reachable_storage() const {
    return matrix.reachable_storage();
  }
  [[nodiscard]] Element operator()(index_t row, index_t column) const {
    if (row == column && diagonal == DenseBlasDiagonal::kUnit) {
      return Element{1};
    }
    if ((triangle == DenseBlasTriangle::kUpper && row > column) ||
        (triangle == DenseBlasTriangle::kLower && row < column)) {
      return Element{0};
    }
    return PackedStoredAt(matrix, triangle, row, column);
  }
};

template <typename Element, typename Access>
Element OperationAt(const Access& access, DenseBlasTranspose transpose,
                    index_t row, index_t column) {
  Element value = transpose == DenseBlasTranspose::kNone
                      ? access(row, column)
                      // Transpose intentionally reverses the matrix indices.
                      // NOLINTNEXTLINE(readability-suspicious-call-argument)
                      : access(column, row);
  return MaybeConjugate(value,
                        transpose == DenseBlasTranspose::kConjugateTranspose);
}

template <typename Element, typename Access>
Status TriangularOperation(const ExecutionContext& context,
                           DenseBlasTriangle triangle,
                           DenseBlasTranspose transpose,
                           DenseBlasDiagonal diagonal, Access access,
                           DenseBlasVectorView<Element> vector, bool solve) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (!IsValidTriangle(triangle) || !IsValidTranspose(transpose) ||
      !IsValidDiagonal(diagonal)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A triangular operation flag is invalid");
  }
  for (Status status :
       {ValidateHostStorage(access), ValidateHostStorage(vector)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (vector.size() != access.order()) {
    return Status(ErrorCode::kShape,
                  "A triangular matrix and vector have different orders");
  }
  if (Overlap(access.reachable_storage(), vector.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "A triangular matrix cannot overlap its vector");
  }
  const bool effective_upper = transpose == DenseBlasTranspose::kNone
                                   ? triangle == DenseBlasTriangle::kUpper
                                   : triangle == DenseBlasTriangle::kLower;
  const index_t order = access.order();
  if (!solve) {
    for (index_t iteration = 0; iteration < order; ++iteration) {
      const index_t row = effective_upper ? iteration : order - 1 - iteration;
      Element result{0};
      const index_t begin = effective_upper ? row : 0;
      const index_t end = effective_upper ? order : row + 1;
      for (index_t column = begin; column < end; ++column) {
        result += OperationAt<Element>(access, transpose, row, column) *
                  VectorAt(vector, column);
      }
      VectorAt(vector, row) = result;
    }
    return Status::Ok();
  }

  for (index_t iteration = 0; iteration < order; ++iteration) {
    const index_t row = effective_upper ? order - 1 - iteration : iteration;
    Element result = VectorAt(vector, row);
    const index_t begin = effective_upper ? row + 1 : 0;
    const index_t end = effective_upper ? order : row;
    for (index_t column = begin; column < end; ++column) {
      result -= OperationAt<Element>(access, transpose, row, column) *
                VectorAt(vector, column);
    }
    if (diagonal == DenseBlasDiagonal::kNonUnit) {
      result /= OperationAt<Element>(access, transpose, row, row);
    }
    VectorAt(vector, row) = result;
  }
  return Status::Ok();
}

template <typename Element, typename Matrix>
Status ValidateRankUpdate(const ExecutionContext& context,
                          DenseBlasTriangle triangle,
                          DenseBlasVectorView<const Element> x,
                          DenseBlasVectorView<const Element> y, Matrix matrix,
                          bool triangular) {
  Status context_status = ValidateContext(context);
  if (!context_status.ok()) {
    return context_status;
  }
  if (triangular && !IsValidTriangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument,
                  "A rank-update triangle value is invalid");
  }
  for (Status status : {ValidateHostStorage(x), ValidateHostStorage(y),
                        ValidateHostStorage(matrix)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (Overlap(x.reachable_storage(), matrix.reachable_storage()) ||
      Overlap(y.reachable_storage(), matrix.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument,
                  "A rank-update matrix overlaps an input vector");
  }
  return Status::Ok();
}

template <typename Element, typename Matrix, typename Reference>
Status RankOne(const ExecutionContext& context, DenseBlasTriangle triangle,
               Element alpha, DenseBlasVectorView<const Element> x,
               Matrix matrix, bool hermitian, Reference reference) {
  Status status = ValidateRankUpdate(context, triangle, x, x, matrix, true);
  if (!status.ok()) {
    return status;
  }
  if (x.size() != matrix.order()) {
    return Status(ErrorCode::kShape,
                  "A rank-update vector has the wrong length");
  }
  if (alpha == Element{0}) {
    return Status::Ok();
  }
  for (index_t row = 0; row < matrix.order(); ++row) {
    const index_t begin = triangle == DenseBlasTriangle::kUpper ? row : 0;
    const index_t end =
        triangle == DenseBlasTriangle::kUpper ? matrix.order() : row + 1;
    for (index_t column = begin; column < end; ++column) {
      Element update =
          alpha * VectorAt(x, row) *
          (hermitian ? Conjugate(VectorAt(x, column)) : VectorAt(x, column));
      Element& destination = reference(row, column);
      destination += update;
      if constexpr (DenseBlasComplex<Element>) {
        if (hermitian && row == column) {
          destination = Element{destination.real(), 0};
        }
      }
    }
  }
  return Status::Ok();
}

template <typename Element, typename Matrix, typename Reference>
Status RankTwo(const ExecutionContext& context, DenseBlasTriangle triangle,
               Element alpha, DenseBlasVectorView<const Element> x,
               DenseBlasVectorView<const Element> y, Matrix matrix,
               bool hermitian, Reference reference) {
  Status status = ValidateRankUpdate(context, triangle, x, y, matrix, true);
  if (!status.ok()) {
    return status;
  }
  if (x.size() != matrix.order() || y.size() != matrix.order()) {
    return Status(ErrorCode::kShape,
                  "A rank-two update vector has the wrong length");
  }
  if (alpha == Element{0}) {
    return Status::Ok();
  }
  for (index_t row = 0; row < matrix.order(); ++row) {
    const index_t begin = triangle == DenseBlasTriangle::kUpper ? row : 0;
    const index_t end =
        triangle == DenseBlasTriangle::kUpper ? matrix.order() : row + 1;
    for (index_t column = begin; column < end; ++column) {
      Element update =
          alpha * VectorAt(x, row) *
          (hermitian ? Conjugate(VectorAt(y, column)) : VectorAt(y, column));
      update +=
          (hermitian ? Conjugate(alpha) : alpha) * VectorAt(y, row) *
          (hermitian ? Conjugate(VectorAt(x, column)) : VectorAt(x, column));
      Element& destination = reference(row, column);
      destination += update;
      if constexpr (DenseBlasComplex<Element>) {
        if (hermitian && row == column) {
          destination = Element{destination.real(), 0};
        }
      }
    }
  }
  return Status::Ok();
}

}  // namespace
}  // namespace internal_dense_blas

template <DenseBlasScalar Element>
Status Gemv(const ExecutionContext& context, DenseBlasTranspose transpose,
            Element alpha, DenseBlasMatrixView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  return internal_dense_blas::GeneralMv(
      context, transpose, alpha, matrix, input, beta, output,
      [matrix](index_t row, index_t column) {
        return internal_dense_blas::MatrixAt(matrix, row, column);
      });
}

template <DenseBlasScalar Element>
Status Gbmv(const ExecutionContext& context, DenseBlasTranspose transpose,
            Element alpha, DenseBlasBandMatrixView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  return internal_dense_blas::GeneralMv(
      context, transpose, alpha, matrix, input, beta, output,
      [matrix](index_t row, index_t column) {
        return internal_dense_blas::GeneralBandAt(matrix, row, column);
      });
}

template <DenseBlasComplex Element>
Status Hemv(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasMatrixView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A Hermitian matrix must be square");
  }
  struct View {
    DenseBlasMatrixView<const Element> matrix;
    [[nodiscard]] extent_t order() const { return matrix.rows(); }
    [[nodiscard]] MemorySpace memory_space() const {
      return matrix.memory_space();
    }
    [[nodiscard]] ConstMemoryView reachable_storage() const {
      return matrix.reachable_storage();
    }
  } view{matrix};
  return internal_dense_blas::StructuredMv(
      context, triangle, alpha, view, input, beta, output,
      [matrix, triangle](index_t row, index_t column) {
        return internal_dense_blas::SymmetricAt(matrix, triangle, row, column,
                                                true);
      });
}

template <DenseBlasComplex Element>
Status Hbmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasTriangularBandView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  return internal_dense_blas::StructuredMv(
      context, triangle, alpha, matrix, input, beta, output,
      [matrix, triangle](index_t row, index_t column) {
        return internal_dense_blas::SymmetricBandAt(matrix, triangle, row,
                                                    column, true);
      });
}

template <DenseBlasComplex Element>
Status Hpmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasPackedMatrixView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  return internal_dense_blas::StructuredMv(
      context, triangle, alpha, matrix, input, beta, output,
      [matrix, triangle](index_t row, index_t column) {
        return internal_dense_blas::SymmetricPackedAt(matrix, triangle, row,
                                                      column, true);
      });
}

template <DenseBlasReal Element>
Status Symv(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasMatrixView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A symmetric matrix must be square");
  }
  struct View {
    DenseBlasMatrixView<const Element> matrix;
    [[nodiscard]] extent_t order() const { return matrix.rows(); }
    [[nodiscard]] MemorySpace memory_space() const {
      return matrix.memory_space();
    }
    [[nodiscard]] ConstMemoryView reachable_storage() const {
      return matrix.reachable_storage();
    }
  } view{matrix};
  return internal_dense_blas::StructuredMv(
      context, triangle, alpha, view, input, beta, output,
      [matrix, triangle](index_t row, index_t column) {
        return internal_dense_blas::SymmetricAt(matrix, triangle, row, column,
                                                false);
      });
}

template <DenseBlasReal Element>
Status Sbmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasTriangularBandView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  return internal_dense_blas::StructuredMv(
      context, triangle, alpha, matrix, input, beta, output,
      [matrix, triangle](index_t row, index_t column) {
        return internal_dense_blas::SymmetricBandAt(matrix, triangle, row,
                                                    column, false);
      });
}

template <DenseBlasReal Element>
Status Spmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasPackedMatrixView<const Element> matrix,
            DenseBlasVectorView<const Element> input, Element beta,
            DenseBlasVectorView<Element> output) {
  return internal_dense_blas::StructuredMv(
      context, triangle, alpha, matrix, input, beta, output,
      [matrix, triangle](index_t row, index_t column) {
        return internal_dense_blas::SymmetricPackedAt(matrix, triangle, row,
                                                      column, false);
      });
}

template <DenseBlasScalar Element>
Status Trmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
            DenseBlasMatrixView<const Element> matrix,
            DenseBlasVectorView<Element> vector) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A triangular matrix must be square");
  }
  return internal_dense_blas::TriangularOperation<Element>(
      context, triangle, transpose, diagonal,
      internal_dense_blas::FullTriangularAccess<Element>{matrix, triangle,
                                                         diagonal},
      vector, false);
}

template <DenseBlasScalar Element>
Status Tbmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
            DenseBlasTriangularBandView<const Element> matrix,
            DenseBlasVectorView<Element> vector) {
  return internal_dense_blas::TriangularOperation<Element>(
      context, triangle, transpose, diagonal,
      internal_dense_blas::BandTriangularAccess<Element>{matrix, triangle,
                                                         diagonal},
      vector, false);
}

template <DenseBlasScalar Element>
Status Tpmv(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
            DenseBlasPackedMatrixView<const Element> matrix,
            DenseBlasVectorView<Element> vector) {
  return internal_dense_blas::TriangularOperation<Element>(
      context, triangle, transpose, diagonal,
      internal_dense_blas::PackedTriangularAccess<Element>{matrix, triangle,
                                                           diagonal},
      vector, false);
}

template <DenseBlasScalar Element>
Status Trsv(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
            DenseBlasMatrixView<const Element> matrix,
            DenseBlasVectorView<Element> vector) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A triangular matrix must be square");
  }
  return internal_dense_blas::TriangularOperation<Element>(
      context, triangle, transpose, diagonal,
      internal_dense_blas::FullTriangularAccess<Element>{matrix, triangle,
                                                         diagonal},
      vector, true);
}

template <DenseBlasScalar Element>
Status Tbsv(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
            DenseBlasTriangularBandView<const Element> matrix,
            DenseBlasVectorView<Element> vector) {
  return internal_dense_blas::TriangularOperation<Element>(
      context, triangle, transpose, diagonal,
      internal_dense_blas::BandTriangularAccess<Element>{matrix, triangle,
                                                         diagonal},
      vector, true);
}

template <DenseBlasScalar Element>
Status Tpsv(const ExecutionContext& context, DenseBlasTriangle triangle,
            DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
            DenseBlasPackedMatrixView<const Element> matrix,
            DenseBlasVectorView<Element> vector) {
  return internal_dense_blas::TriangularOperation<Element>(
      context, triangle, transpose, diagonal,
      internal_dense_blas::PackedTriangularAccess<Element>{matrix, triangle,
                                                           diagonal},
      vector, true);
}

template <DenseBlasReal Element>
Status Ger(const ExecutionContext& context, Element alpha,
           DenseBlasVectorView<const Element> x,
           DenseBlasVectorView<const Element> y,
           DenseBlasMatrixView<Element> matrix) {
  Status status = internal_dense_blas::ValidateRankUpdate(
      context, DenseBlasTriangle::kUpper, x, y, matrix, false);
  if (!status.ok()) {
    return status;
  }
  if (x.size() != matrix.rows() || y.size() != matrix.columns()) {
    return Status(ErrorCode::kShape, "Ger operand shapes do not match");
  }
  if (alpha == Element{0}) {
    return Status::Ok();
  }
  for (index_t column = 0; column < matrix.columns(); ++column) {
    for (index_t row = 0; row < matrix.rows(); ++row) {
      internal_dense_blas::MatrixAt(matrix, row, column) +=
          alpha * internal_dense_blas::VectorAt(x, row) *
          internal_dense_blas::VectorAt(y, column);
    }
  }
  return Status::Ok();
}

template <DenseBlasComplex Element>
Status Geru(const ExecutionContext& context, Element alpha,
            DenseBlasVectorView<const Element> x,
            DenseBlasVectorView<const Element> y,
            DenseBlasMatrixView<Element> matrix) {
  Status status = internal_dense_blas::ValidateRankUpdate(
      context, DenseBlasTriangle::kUpper, x, y, matrix, false);
  if (!status.ok()) {
    return status;
  }
  if (x.size() != matrix.rows() || y.size() != matrix.columns()) {
    return Status(ErrorCode::kShape, "Geru operand shapes do not match");
  }
  if (alpha == Element{0}) {
    return Status::Ok();
  }
  for (index_t column = 0; column < matrix.columns(); ++column) {
    for (index_t row = 0; row < matrix.rows(); ++row) {
      internal_dense_blas::MatrixAt(matrix, row, column) +=
          alpha * internal_dense_blas::VectorAt(x, row) *
          internal_dense_blas::VectorAt(y, column);
    }
  }
  return Status::Ok();
}

template <DenseBlasComplex Element>
Status Gerc(const ExecutionContext& context, Element alpha,
            DenseBlasVectorView<const Element> x,
            DenseBlasVectorView<const Element> y,
            DenseBlasMatrixView<Element> matrix) {
  Status status = internal_dense_blas::ValidateRankUpdate(
      context, DenseBlasTriangle::kUpper, x, y, matrix, false);
  if (!status.ok()) {
    return status;
  }
  if (x.size() != matrix.rows() || y.size() != matrix.columns()) {
    return Status(ErrorCode::kShape, "Gerc operand shapes do not match");
  }
  if (alpha == Element{0}) {
    return Status::Ok();
  }
  for (index_t column = 0; column < matrix.columns(); ++column) {
    for (index_t row = 0; row < matrix.rows(); ++row) {
      internal_dense_blas::MatrixAt(matrix, row, column) +=
          alpha * internal_dense_blas::VectorAt(x, row) *
          internal_dense_blas::Conjugate(
              internal_dense_blas::VectorAt(y, column));
    }
  }
  return Status::Ok();
}

template <DenseBlasComplex Element>
Status Her(const ExecutionContext& context, DenseBlasTriangle triangle,
           DenseBlasRealType<Element> alpha,
           DenseBlasVectorView<const Element> x,
           DenseBlasMatrixView<Element> matrix) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A Hermitian matrix must be square");
  }
  return internal_dense_blas::RankOne(
      context, triangle, Element{alpha}, x,
      [&]() {
        struct View {
          DenseBlasMatrixView<Element> matrix;
          [[nodiscard]] extent_t order() const { return matrix.rows(); }
          [[nodiscard]] MemorySpace memory_space() const {
            return matrix.memory_space();
          }
          [[nodiscard]] ConstMemoryView reachable_storage() const {
            return matrix.reachable_storage();
          }
        };
        return View{matrix};
      }(),
      true,
      [matrix](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::MatrixAt(matrix, row, column);
      });
}

template <DenseBlasComplex Element>
Status Hpr(const ExecutionContext& context, DenseBlasTriangle triangle,
           DenseBlasRealType<Element> alpha,
           DenseBlasVectorView<const Element> x,
           DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_blas::RankOne(
      context, triangle, Element{alpha}, x, matrix, true,
      [matrix, triangle](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::PackedStoredAt(matrix, triangle, row,
                                                   column);
      });
}

template <DenseBlasComplex Element>
Status Her2(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasVectorView<const Element> x,
            DenseBlasVectorView<const Element> y,
            DenseBlasMatrixView<Element> matrix) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A Hermitian matrix must be square");
  }
  struct View {
    DenseBlasMatrixView<Element> matrix;
    [[nodiscard]] extent_t order() const { return matrix.rows(); }
    [[nodiscard]] MemorySpace memory_space() const {
      return matrix.memory_space();
    }
    [[nodiscard]] ConstMemoryView reachable_storage() const {
      return matrix.reachable_storage();
    }
  } view{matrix};
  return internal_dense_blas::RankTwo(
      context, triangle, alpha, x, y, view, true,
      [matrix](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::MatrixAt(matrix, row, column);
      });
}

template <DenseBlasComplex Element>
Status Hpr2(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasVectorView<const Element> x,
            DenseBlasVectorView<const Element> y,
            DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_blas::RankTwo(
      context, triangle, alpha, x, y, matrix, true,
      [matrix, triangle](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::PackedStoredAt(matrix, triangle, row,
                                                   column);
      });
}

template <DenseBlasReal Element>
Status Syr(const ExecutionContext& context, DenseBlasTriangle triangle,
           Element alpha, DenseBlasVectorView<const Element> x,
           DenseBlasMatrixView<Element> matrix) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A symmetric matrix must be square");
  }
  struct View {
    DenseBlasMatrixView<Element> matrix;
    [[nodiscard]] extent_t order() const { return matrix.rows(); }
    [[nodiscard]] MemorySpace memory_space() const {
      return matrix.memory_space();
    }
    [[nodiscard]] ConstMemoryView reachable_storage() const {
      return matrix.reachable_storage();
    }
  } view{matrix};
  return internal_dense_blas::RankOne(
      context, triangle, alpha, x, view, false,
      [matrix](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::MatrixAt(matrix, row, column);
      });
}

template <DenseBlasReal Element>
Status Spr(const ExecutionContext& context, DenseBlasTriangle triangle,
           Element alpha, DenseBlasVectorView<const Element> x,
           DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_blas::RankOne(
      context, triangle, alpha, x, matrix, false,
      [matrix, triangle](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::PackedStoredAt(matrix, triangle, row,
                                                   column);
      });
}

template <DenseBlasReal Element>
Status Syr2(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasVectorView<const Element> x,
            DenseBlasVectorView<const Element> y,
            DenseBlasMatrixView<Element> matrix) {
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape, "A symmetric matrix must be square");
  }
  struct View {
    DenseBlasMatrixView<Element> matrix;
    [[nodiscard]] extent_t order() const { return matrix.rows(); }
    [[nodiscard]] MemorySpace memory_space() const {
      return matrix.memory_space();
    }
    [[nodiscard]] ConstMemoryView reachable_storage() const {
      return matrix.reachable_storage();
    }
  } view{matrix};
  return internal_dense_blas::RankTwo(
      context, triangle, alpha, x, y, view, false,
      [matrix](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::MatrixAt(matrix, row, column);
      });
}

template <DenseBlasReal Element>
Status Spr2(const ExecutionContext& context, DenseBlasTriangle triangle,
            Element alpha, DenseBlasVectorView<const Element> x,
            DenseBlasVectorView<const Element> y,
            DenseBlasPackedMatrixView<Element> matrix) {
  return internal_dense_blas::RankTwo(
      context, triangle, alpha, x, y, matrix, false,
      [matrix, triangle](index_t row, index_t column) mutable -> Element& {
        return internal_dense_blas::PackedStoredAt(matrix, triangle, row,
                                                   column);
      });
}

#define ASC_INSTANTIATE_GENERAL(Type)                                         \
  template ASC_DENSE_EXPORT Status Gemv<Type>(                                \
      const ExecutionContext&, DenseBlasTranspose, Type,                      \
      DenseBlasMatrixView<const Type>, DenseBlasVectorView<const Type>, Type, \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Gbmv<Type>(                                \
      const ExecutionContext&, DenseBlasTranspose, Type,                      \
      DenseBlasBandMatrixView<const Type>, DenseBlasVectorView<const Type>,   \
      Type, DenseBlasVectorView<Type>);                                       \
  template ASC_DENSE_EXPORT Status Trmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose,         \
      DenseBlasDiagonal, DenseBlasMatrixView<const Type>,                     \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Tbmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose,         \
      DenseBlasDiagonal, DenseBlasTriangularBandView<const Type>,             \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Tpmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose,         \
      DenseBlasDiagonal, DenseBlasPackedMatrixView<const Type>,               \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Trsv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose,         \
      DenseBlasDiagonal, DenseBlasMatrixView<const Type>,                     \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Tbsv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose,         \
      DenseBlasDiagonal, DenseBlasTriangularBandView<const Type>,             \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Tpsv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, DenseBlasTranspose,         \
      DenseBlasDiagonal, DenseBlasPackedMatrixView<const Type>,               \
      DenseBlasVectorView<Type>)

#define ASC_INSTANTIATE_REAL(Type)                                            \
  ASC_INSTANTIATE_GENERAL(Type);                                              \
  template ASC_DENSE_EXPORT Status Symv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasMatrixView<const Type>, DenseBlasVectorView<const Type>, Type, \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Sbmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasTriangularBandView<const Type>,                                \
      DenseBlasVectorView<const Type>, Type, DenseBlasVectorView<Type>);      \
  template ASC_DENSE_EXPORT Status Spmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasPackedMatrixView<const Type>, DenseBlasVectorView<const Type>, \
      Type, DenseBlasVectorView<Type>);                                       \
  template ASC_DENSE_EXPORT Status Ger<Type>(                                 \
      const ExecutionContext&, Type, DenseBlasVectorView<const Type>,         \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template ASC_DENSE_EXPORT Status Syr<Type>(                                 \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template ASC_DENSE_EXPORT Status Spr<Type>(                                 \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasVectorView<const Type>, DenseBlasPackedMatrixView<Type>);      \
  template ASC_DENSE_EXPORT Status Syr2<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasMatrixView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Spr2<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasPackedMatrixView<Type>)

#define ASC_INSTANTIATE_COMPLEX(Type, Real)                                   \
  ASC_INSTANTIATE_GENERAL(Type);                                              \
  template ASC_DENSE_EXPORT Status Hemv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasMatrixView<const Type>, DenseBlasVectorView<const Type>, Type, \
      DenseBlasVectorView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Hbmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasTriangularBandView<const Type>,                                \
      DenseBlasVectorView<const Type>, Type, DenseBlasVectorView<Type>);      \
  template ASC_DENSE_EXPORT Status Hpmv<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasPackedMatrixView<const Type>, DenseBlasVectorView<const Type>, \
      Type, DenseBlasVectorView<Type>);                                       \
  template ASC_DENSE_EXPORT Status Geru<Type>(                                \
      const ExecutionContext&, Type, DenseBlasVectorView<const Type>,         \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template ASC_DENSE_EXPORT Status Gerc<Type>(                                \
      const ExecutionContext&, Type, DenseBlasVectorView<const Type>,         \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template ASC_DENSE_EXPORT Status Her<Type>(                                 \
      const ExecutionContext&, DenseBlasTriangle, Real,                       \
      DenseBlasVectorView<const Type>, DenseBlasMatrixView<Type>);            \
  template ASC_DENSE_EXPORT Status Hpr<Type>(                                 \
      const ExecutionContext&, DenseBlasTriangle, Real,                       \
      DenseBlasVectorView<const Type>, DenseBlasPackedMatrixView<Type>);      \
  template ASC_DENSE_EXPORT Status Her2<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasMatrixView<Type>);                                             \
  template ASC_DENSE_EXPORT Status Hpr2<Type>(                                \
      const ExecutionContext&, DenseBlasTriangle, Type,                       \
      DenseBlasVectorView<const Type>, DenseBlasVectorView<const Type>,       \
      DenseBlasPackedMatrixView<Type>)

ASC_INSTANTIATE_REAL(float);
ASC_INSTANTIATE_REAL(double);
ASC_INSTANTIATE_COMPLEX(std::complex<float>, float);
ASC_INSTANTIATE_COMPLEX(std::complex<double>, double);

#undef ASC_INSTANTIATE_COMPLEX
#undef ASC_INSTANTIATE_GENERAL
#undef ASC_INSTANTIATE_REAL

}  // namespace asc
