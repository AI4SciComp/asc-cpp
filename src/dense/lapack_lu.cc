#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/lu.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"

namespace asc {
namespace {

template <typename Element>
constexpr LapackScalarKind ScalarKind() {
  if constexpr (std::is_same_v<Element, float>) {
    return LapackScalarKind::kF32;
  } else if constexpr (std::is_same_v<Element, double>) {
    return LapackScalarKind::kF64;
  } else if constexpr (std::is_same_v<Element, std::complex<float>>) {
    return LapackScalarKind::kC64;
  } else {
    return LapackScalarKind::kC128;
  }
}

template <typename Element>
Status StartReport(std::string_view operation, std::span<const extent_t> shape,
                   std::span<const std::int64_t> options,
                   LapackReport& report) {
  report = {};
  auto identity = LapackPlanIdentity::Create(operation, ScalarKind<Element>(),
                                             shape, options, {});
  if (!identity.ok()) {
    return identity.status();
  }
  InitializeLapackReport(*identity, report);
  return Status::Ok();
}

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto first_begin = reinterpret_cast<std::uintptr_t>(first.data());
  const auto second_begin = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 &&
         first_begin < second_begin + second.size() &&
         second_begin < first_begin + first.size();
}

Status ValidateExecution(const ExecutionContext& context,
                         std::span<const MemorySpace> placements) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported);
  }
  for (MemorySpace placement : placements) {
    if (!context.CanAccess(placement)) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  return Status::Ok();
}

template <typename Element>
Element& At(DenseBlasMatrixView<Element> matrix, index_t row, index_t column) {
  const index_t offset = matrix.layout() == DenseBlasLayout::kRowMajor
                             ? row * matrix.leading_dimension() + column
                             : column * matrix.leading_dimension() + row;
  return matrix.data()[offset];
}

template <typename Element>
DenseBlasRealType<Element> PivotMagnitude(Element value) {
  if constexpr (DenseBlasComplex<Element>) {
    return std::abs(value.real()) + std::abs(value.imag());
  } else {
    return std::abs(value);
  }
}

template <typename Element>
void SwapRows(DenseBlasMatrixView<Element> matrix, index_t first,
              index_t second) {
  if (first == second) {
    return;
  }
  for (index_t column = 0; column < matrix.columns(); ++column) {
    std::swap(At(matrix, first, column), At(matrix, second, column));
  }
}

template <typename Element>
void FactorColumn(DenseBlasMatrixView<Element> matrix, index_t column,
                  DenseBlasVectorView<index_t> pivots, LapackReport& report) {
  index_t selected = column;
  auto largest = PivotMagnitude(At(matrix, column, column));
  for (index_t row = column + 1; row < matrix.rows(); ++row) {
    const auto magnitude = PivotMagnitude(At(matrix, row, column));
    if (magnitude > largest) {
      largest = magnitude;
      selected = row;
    }
  }
  pivots.data()[column] = selected + 1;
  const Element pivot = At(matrix, selected, column);
  if (pivot != Element{0}) {
    SwapRows(matrix, column, selected);
    for (index_t row = column + 1; row < matrix.rows(); ++row) {
      // Direct division avoids forming an overflowing reciprocal of a nonzero
      // subnormal pivot. Exact zero is the only singularity policy here.
      At(matrix, row, column) /= pivot;
    }
  } else if (!report.diagnostic_index.has_value()) {
    report.diagnostic_index = column;
  }
  for (index_t trailing = column + 1; trailing < matrix.columns(); ++trailing) {
    // The pivot column index also selects the corresponding U row.
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    const Element upper = At(matrix, column, trailing);
    for (index_t row = column + 1; row < matrix.rows(); ++row) {
      At(matrix, row, trailing) -= At(matrix, row, column) * upper;
    }
  }
}

template <typename Element>
Status GetrfImpl(const ExecutionContext& context,
                 DenseBlasMatrixView<Element> matrix,
                 DenseBlasVectorView<index_t> pivots, LapackReport& report,
                 std::string_view operation) {
  Status status = StartReport<Element>(
      operation, std::array{matrix.rows(), matrix.columns(), pivots.size()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension(),
                                  pivots.increment()},
      report);
  if (!status.ok()) {
    return status;
  }
  status = ValidateExecution(
      context, std::array{matrix.memory_space(), pivots.memory_space()});
  if (!status.ok()) {
    return status;
  }
  const extent_t count = std::min(matrix.rows(), matrix.columns());
  if (pivots.size() != count) {
    return Status(ErrorCode::kShape);
  }
  if (pivots.increment() != 1 ||
      Overlap(matrix.reachable_storage(), pivots.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  report.factor_family = LapackFactorFamily::kLuPartialPivot;
  for (index_t column = 0; column < count; ++column) {
    FactorColumn(matrix, column, pivots, report);
  }
  if (report.diagnostic_index.has_value()) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename Element>
Status ValidateSolve(const ExecutionContext& context,
                     DenseBlasTranspose transpose,
                     LapackLuFactorView<Element> factor,
                     DenseBlasMatrixView<Element> rhs) {
  const auto matrix = factor.factors();
  const auto pivots = factor.pivots();
  Status status = ValidateExecution(
      context, std::array{matrix.memory_space(), rhs.memory_space(),
                          pivots.reachable_storage().space()});
  if (!status.ok()) {
    return status;
  }
  if (transpose != DenseBlasTranspose::kNone &&
      transpose != DenseBlasTranspose::kTranspose &&
      transpose != DenseBlasTranspose::kConjugateTranspose) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || rhs.rows() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (factor.provider() != LapackProviderIdentity{}) {
    return Status(ErrorCode::kInvalidState);
  }
  if (Overlap(matrix.reachable_storage(), rhs.reachable_storage()) ||
      Overlap(pivots.reachable_storage(), rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return ValidateLuPivots(pivots, matrix.rows());
}

template <typename Element>
Element OperationAt(DenseBlasMatrixView<const Element> matrix,
                    DenseBlasTranspose transpose, index_t row, index_t column) {
  if (transpose == DenseBlasTranspose::kNone) {
    return At(matrix, row, column);
  }
  // Transposition intentionally exchanges the row and column coordinates.
  // NOLINTNEXTLINE(readability-suspicious-call-argument)
  const Element value = At(matrix, column, row);
  if constexpr (DenseBlasComplex<Element>) {
    return transpose == DenseBlasTranspose::kConjugateTranspose
               ? std::conj(value)
               : value;
  } else {
    return value;
  }
}

template <typename Element>
Status Substitute(const ExecutionContext& context, DenseBlasTriangle triangle,
                  DenseBlasTranspose transpose, DenseBlasDiagonal diagonal,
                  DenseBlasMatrixView<const Element> matrix,
                  DenseBlasMatrixView<Element> rhs) {
  if (matrix.layout() == rhs.layout()) {
    return Trsm(context, DenseBlasSide::kLeft, triangle, transpose, diagonal,
                Element{1}, matrix, rhs);
  }
  // Existing BLAS requires a common layout. Direct substitution preserves the
  // broader native LAPACK descriptor contract without packing mixed layouts.
  const bool upper = transpose == DenseBlasTranspose::kNone
                         ? triangle == DenseBlasTriangle::kUpper
                         : triangle == DenseBlasTriangle::kLower;
  for (index_t column = 0; column < rhs.columns(); ++column) {
    for (index_t step = 0; step < rhs.rows(); ++step) {
      const index_t row = upper ? rhs.rows() - 1 - step : step;
      Element value = At(rhs, row, column);
      const index_t begin = upper ? row + 1 : 0;
      const index_t end = upper ? rhs.rows() : row;
      for (index_t index = begin; index < end; ++index) {
        value -=
            OperationAt(matrix, transpose, row, index) * At(rhs, index, column);
      }
      if (diagonal == DenseBlasDiagonal::kNonUnit) {
        value /= OperationAt(matrix, transpose, row, row);
      }
      At(rhs, row, column) = value;
    }
  }
  return Status::Ok();
}

template <typename Element>
Status SolveTriangular(const ExecutionContext& context,
                       DenseBlasTranspose transpose,
                       LapackLuFactorView<Element> factor,
                       DenseBlasMatrixView<Element> rhs) {
  const auto pivots = factor.pivots().values();
  if (transpose == DenseBlasTranspose::kNone) {
    for (index_t row = 0; row < rhs.rows(); ++row) {
      SwapRows(rhs, row, pivots[row] - 1);
    }
  }
  const bool normal = transpose == DenseBlasTranspose::kNone;
  // All of TRSM's structural preconditions have already been checked, before
  // any swaps/writes. The BLAS primitive does not allocate or dispatch abroad.
  Status status = Substitute(
      context, normal ? DenseBlasTriangle::kLower : DenseBlasTriangle::kUpper,
      transpose,
      normal ? DenseBlasDiagonal::kUnit : DenseBlasDiagonal::kNonUnit,
      factor.factors(), rhs);
  if (!status.ok()) {
    return status;
  }
  status = Substitute(
      context, normal ? DenseBlasTriangle::kUpper : DenseBlasTriangle::kLower,
      transpose,
      normal ? DenseBlasDiagonal::kNonUnit : DenseBlasDiagonal::kUnit,
      factor.factors(), rhs);
  if (!status.ok()) {
    return status;
  }
  if (!normal) {
    for (index_t count = rhs.rows(); count > 0; --count) {
      SwapRows(rhs, count - 1, pivots[count - 1] - 1);
    }
  }
  return Status::Ok();
}

template <typename Element>
Status GetrsImpl(const ExecutionContext& context, DenseBlasTranspose transpose,
                 LapackLuFactorView<Element> factor,
                 DenseBlasMatrixView<Element> rhs, LapackReport& report,
                 std::string_view operation) {
  Status status = StartReport<Element>(
      operation,
      std::array{factor.factors().rows(), factor.factors().columns(),
                 rhs.rows(), rhs.columns()},
      std::array<std::int64_t, 1>{static_cast<std::int64_t>(transpose)},
      report);
  if (!status.ok()) {
    return status;
  }
  status = ValidateSolve(context, transpose, factor, rhs);
  if (!status.ok()) {
    return status;
  }
  if (rhs.rows() != 0 && rhs.columns() != 0) {
    for (index_t row = 0; row < rhs.rows(); ++row) {
      if (At(factor.factors(), row, row) == Element{0}) {
        report.outcome = LapackOutcome::kSingular;
        report.diagnostic_index = row;
        return Status(ErrorCode::kNumerical);
      }
    }
    status = SolveTriangular(context, transpose, factor, rhs);
    if (!status.ok()) {
      report.output_validity = LapackOutputValidity::kUnusable;
      return status;
    }
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

}  // namespace

Status Getrf(const ExecutionContext& context, DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> pivots, LapackReport& report) {
  return GetrfImpl(context, matrix, pivots, report, "sgetrf");
}
Status Getrf(const ExecutionContext& context,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> pivots, LapackReport& report) {
  return GetrfImpl(context, matrix, pivots, report, "dgetrf");
}
Status Getrf(const ExecutionContext& context,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots, LapackReport& report) {
  return GetrfImpl(context, matrix, pivots, report, "cgetrf");
}
Status Getrf(const ExecutionContext& context,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots, LapackReport& report) {
  return GetrfImpl(context, matrix, pivots, report, "zgetrf");
}
Status Getrs(const ExecutionContext& context, DenseBlasTranspose transpose,
             LapackLuFactorView<float> factor, DenseBlasMatrixView<float> rhs,
             LapackReport& report) {
  return GetrsImpl(context, transpose, factor, rhs, report, "sgetrs");
}
Status Getrs(const ExecutionContext& context, DenseBlasTranspose transpose,
             LapackLuFactorView<double> factor, DenseBlasMatrixView<double> rhs,
             LapackReport& report) {
  return GetrsImpl(context, transpose, factor, rhs, report, "dgetrs");
}
Status Getrs(const ExecutionContext& context, DenseBlasTranspose transpose,
             LapackLuFactorView<std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             LapackReport& report) {
  return GetrsImpl(context, transpose, factor, rhs, report, "cgetrs");
}
Status Getrs(const ExecutionContext& context, DenseBlasTranspose transpose,
             LapackLuFactorView<std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             LapackReport& report) {
  return GetrsImpl(context, transpose, factor, rhs, report, "zgetrs");
}

}  // namespace asc
