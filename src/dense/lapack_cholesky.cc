#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/cholesky.h"
#include "asc/dense/lapack/factor_view.h"
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

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  const auto first_begin = reinterpret_cast<std::uintptr_t>(first.data());
  const auto second_begin = reinterpret_cast<std::uintptr_t>(second.data());
  return first.size() != 0 && second.size() != 0 &&
         first_begin < second_begin + second.size() &&
         second_begin < first_begin + first.size();
}

bool ReportAliases(const LapackReport& report, ConstMemoryView storage) {
  return Overlap({&report, sizeof(report), MemorySpace::kHost}, storage);
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

Status ValidateExecution(const ExecutionContext& context,
                         std::span<const MemorySpace> placements) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported);
  }
  for (const auto placement : placements) {
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
Element Conjugate(Element value) {
  if constexpr (DenseBlasComplex<Element>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename Element>
DenseBlasRealType<Element> RealPart(Element value) {
  if constexpr (DenseBlasComplex<Element>) {
    return value.real();
  } else {
    return value;
  }
}

template <typename Element>
DenseBlasRealType<Element> SquaredMagnitude(Element value) {
  if constexpr (DenseBlasComplex<Element>) {
    return value.real() * value.real() + value.imag() * value.imag();
  } else {
    return value * value;
  }
}

// Interpret either actual triangle as a mathematical lower factor. This
// changes only index/conjugation access, never packs or reads an omitted entry.
template <typename Element>
std::remove_const_t<Element> LowerValue(DenseBlasMatrixView<Element> matrix,
                                        DenseBlasTriangle triangle, index_t row,
                                        index_t column) {
  if (triangle == DenseBlasTriangle::kLower) {
    return At(matrix, row, column);
  }
  const index_t stored_row = column;
  const index_t stored_column = row;
  return Conjugate(At(matrix, stored_row, stored_column));
}

template <typename Element>
void StoreLower(DenseBlasMatrixView<Element> matrix, DenseBlasTriangle triangle,
                index_t row, index_t column, Element value) {
  if (triangle == DenseBlasTriangle::kLower) {
    At(matrix, row, column) = value;
  } else {
    const index_t stored_row = column;
    const index_t stored_column = row;
    At(matrix, stored_row, stored_column) = Conjugate(value);
  }
}

template <typename Element>
bool FactorColumn(DenseBlasMatrixView<Element> matrix,
                  DenseBlasTriangle triangle, index_t column) {
  using Real = DenseBlasRealType<Element>;
  const index_t pivot_row = column;
  Real pivot = RealPart(At(matrix, column, column));
  for (index_t previous = 0; previous < column; ++previous) {
    pivot -=
        SquaredMagnitude(LowerValue(matrix, triangle, pivot_row, previous));
  }
  if (!(pivot > Real{0})) {
    At(matrix, column, column) = Element{pivot};
    return false;
  }
  const Real root = std::sqrt(pivot);
  At(matrix, column, column) = Element{root};
  for (index_t row = column + 1; row < matrix.rows(); ++row) {
    Element value = LowerValue(matrix, triangle, row, column);
    for (index_t previous = 0; previous < column; ++previous) {
      value -= LowerValue(matrix, triangle, row, previous) *
               Conjugate(LowerValue(matrix, triangle, pivot_row, previous));
    }
    // Direct division does not form an overflowing reciprocal for tiny roots.
    StoreLower(matrix, triangle, row, column, value / root);
  }
  return true;
}

template <typename Element>
Status PotrfImpl(const ExecutionContext& context, DenseBlasTriangle triangle,
                 DenseBlasMatrixView<Element> matrix, LapackReport& report,
                 std::string_view operation) {
  if (ReportAliases(report, matrix.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto status = StartReport<Element>(
      operation, std::array{matrix.rows(), matrix.columns()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension()},
      report);
  if (!status.ok()) {
    return status;
  }
  status = ValidateExecution(context, std::array{matrix.memory_space()});
  if (!status.ok()) {
    return status;
  }
  if (triangle != DenseBlasTriangle::kUpper &&
      triangle != DenseBlasTriangle::kLower) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns()) {
    return Status(ErrorCode::kShape);
  }
  report.factor_family = LapackFactorFamily::kCholesky;
  for (index_t column = 0; column < matrix.columns(); ++column) {
    if (!FactorColumn(matrix, triangle, column)) {
      report.outcome = LapackOutcome::kNotPositiveDefinite;
      report.output_validity = LapackOutputValidity::kDocumentedPartial;
      report.diagnostic_index = column;
      return Status(ErrorCode::kNumerical);
    }
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

template <typename Element>
Status ValidateDiagonal(LapackCholeskyFactorView<Element> factor,
                        LapackReport& report) {
  using Real = DenseBlasRealType<Element>;
  const auto matrix = factor.factors();
  for (index_t row = 0; row < matrix.rows(); ++row) {
    const Element diagonal = At(matrix, row, row);
    const Real real = RealPart(diagonal);
    bool valid = real > Real{0} && std::isfinite(real);
    if constexpr (DenseBlasComplex<Element>) {
      valid = valid && diagonal.imag() == Real{0};
    }
    if (!valid) {
      report.outcome = LapackOutcome::kNotPositiveDefinite;
      report.diagnostic_index = row;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename Element>
void Substitute(LapackCholeskyFactorView<Element> factor,
                DenseBlasMatrixView<Element> rhs) {
  const auto matrix = factor.factors();
  const auto triangle = factor.triangle();
  for (index_t column = 0; column < rhs.columns(); ++column) {
    for (index_t row = 0; row < rhs.rows(); ++row) {
      Element value = At(rhs, row, column);
      for (index_t previous = 0; previous < row; ++previous) {
        value -= LowerValue(matrix, triangle, row, previous) *
                 At(rhs, previous, column);
      }
      At(rhs, row, column) = value / RealPart(At(matrix, row, row));
    }
    for (index_t remaining = rhs.rows(); remaining > 0; --remaining) {
      const index_t row = remaining - 1;
      const index_t factor_column = row;
      Element value = At(rhs, row, column);
      for (index_t following = row + 1; following < rhs.rows(); ++following) {
        value -=
            Conjugate(LowerValue(matrix, triangle, following, factor_column)) *
            At(rhs, following, column);
      }
      At(rhs, row, column) = value / RealPart(At(matrix, row, row));
    }
  }
}

template <typename Element>
Status PotrsImpl(const ExecutionContext& context,
                 LapackCholeskyFactorView<Element> factor,
                 DenseBlasMatrixView<Element> rhs, LapackReport& report,
                 std::string_view operation) {
  const auto matrix = factor.factors();
  if (ReportAliases(report, matrix.reachable_storage()) ||
      ReportAliases(report, rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto status = StartReport<Element>(
      operation,
      std::array{matrix.rows(), matrix.columns(), rhs.rows(), rhs.columns()},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(factor.triangle()),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension(),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      report);
  if (!status.ok()) {
    return status;
  }
  status = ValidateExecution(
      context, std::array{matrix.memory_space(), rhs.memory_space()});
  if (!status.ok()) {
    return status;
  }
  if (factor.provider() != LapackProviderIdentity{}) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (Overlap(matrix.reachable_storage(), rhs.reachable_storage())) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rhs.rows() != 0 && rhs.columns() != 0) {
    status = ValidateDiagonal(factor, report);
    if (!status.ok()) {
      return status;
    }
    Substitute(factor, rhs);
  }
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

}  // namespace

Status Potrf(const ExecutionContext& context, DenseBlasTriangle triangle,
             DenseBlasMatrixView<float> matrix, LapackReport& report) {
  return PotrfImpl(context, triangle, matrix, report, "spotrf");
}
Status Potrf(const ExecutionContext& context, DenseBlasTriangle triangle,
             DenseBlasMatrixView<double> matrix, LapackReport& report) {
  return PotrfImpl(context, triangle, matrix, report, "dpotrf");
}
Status Potrf(const ExecutionContext& context, DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             LapackReport& report) {
  return PotrfImpl(context, triangle, matrix, report, "cpotrf");
}
Status Potrf(const ExecutionContext& context, DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             LapackReport& report) {
  return PotrfImpl(context, triangle, matrix, report, "zpotrf");
}
Status Potrs(const ExecutionContext& context,
             LapackCholeskyFactorView<float> factor,
             DenseBlasMatrixView<float> rhs, LapackReport& report) {
  return PotrsImpl(context, factor, rhs, report, "spotrs");
}
Status Potrs(const ExecutionContext& context,
             LapackCholeskyFactorView<double> factor,
             DenseBlasMatrixView<double> rhs, LapackReport& report) {
  return PotrsImpl(context, factor, rhs, report, "dpotrs");
}
Status Potrs(const ExecutionContext& context,
             LapackCholeskyFactorView<std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             LapackReport& report) {
  return PotrsImpl(context, factor, rhs, report, "cpotrs");
}
Status Potrs(const ExecutionContext& context,
             LapackCholeskyFactorView<std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             LapackReport& report) {
  return PotrsImpl(context, factor, rhs, report, "zpotrs");
}

}  // namespace asc
