#include <algorithm>
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
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/qr.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"

namespace asc {
namespace {

template <typename T>
constexpr LapackScalarKind ScalarKind() {
  if constexpr (std::is_same_v<T, float>) {
    return LapackScalarKind::kF32;
  } else if constexpr (std::is_same_v<T, double>) {
    return LapackScalarKind::kF64;
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return LapackScalarKind::kC64;
  } else {
    return LapackScalarKind::kC128;
  }
}

bool Overlap(ConstMemoryView first, ConstMemoryView second) {
  if (first.size() == 0 || second.size() == 0) {
    return false;
  }
  const auto left = reinterpret_cast<std::uintptr_t>(first.data());
  const auto right = reinterpret_cast<std::uintptr_t>(second.data());
  return left <= right ? right - left < first.size()
                       : left - right < second.size();
}

template <typename T>
Status Start(std::string_view name, std::span<const extent_t> dimensions,
             std::span<const std::int64_t> options,
             std::span<const ConstMemoryView> operands, LapackReport& report) {
  const ConstMemoryView state(&report, sizeof(report), MemorySpace::kHost);
  for (const auto operand : operands) {
    if (Overlap(state, operand)) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  report = {};
  const auto identity = LapackPlanIdentity::Create(name, ScalarKind<T>(),
                                                   dimensions, options, {});
  if (!identity.ok()) {
    return identity.status();
  }
  InitializeLapackReport(*identity, report);
  return Status::Ok();
}

Status ValidateExecution(const ExecutionContext& context,
                         std::span<const ConstMemoryView> operands) {
  if (context.backend() != Backend::kSerial) {
    return Status(ErrorCode::kUnsupported);
  }
  for (const auto operand : operands) {
    if (!context.CanAccess(operand.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  return internal_dense_lapack::Disjoint(operands);
}

template <typename T>
T& At(DenseBlasMatrixView<T> matrix, index_t row, index_t column) {
  const auto offset = matrix.layout() == DenseBlasLayout::kRowMajor
                          ? row * matrix.leading_dimension() + column
                          : column * matrix.leading_dimension() + row;
  return matrix.data()[offset];
}

template <typename T>
T Conjugate(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename T>
bool Finite(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
  } else {
    return std::isfinite(value);
  }
}

template <typename T>
DenseBlasRealType<T> ComponentMagnitude(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::max(std::abs(value.real()), std::abs(value.imag()));
  } else {
    return std::abs(value);
  }
}

template <typename T>
DenseBlasRealType<T> RealPart(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return value.real();
  } else {
    return value;
  }
}

template <typename T>
bool RealValue(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return value.imag() == 0;
  } else {
    return true;
  }
}

template <typename T>
DenseBlasRealType<T> NormWith(DenseBlasRealType<T> previous, T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::hypot(std::hypot(previous, value.real()), value.imag());
  } else {
    return std::hypot(previous, value);
  }
}

template <typename T>
Status ValidateFinite(DenseBlasMatrixView<T> matrix, LapackReport& report) {
  for (index_t column = 0; column < matrix.columns(); ++column) {
    for (index_t row = 0; row < matrix.rows(); ++row) {
      if (!Finite(At(matrix, row, column))) {
        report.diagnostic_index = column;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return Status::Ok();
}

template <typename T>
Status ValidateReflectors(LapackHouseholderQrFactorView<T> factor,
                          LapackReport& report) {
  for (index_t i = 0; i < factor.tau().size(); ++i) {
    const T tau = factor.tau().data()[i];
    if (!Finite(tau)) {
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
    if (tau == T{0}) {
      continue;
    }
    for (index_t row = i + 1; row < factor.factors().rows(); ++row) {
      if (!Finite(At(factor.factors(), row, i))) {
        report.diagnostic_index = i;
        return Status(ErrorCode::kNumerical);
      }
    }
  }
  return Status::Ok();
}

Status NumericalFailure(index_t reflector, LapackReport& report) {
  report.outcome = LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kUnusable;
  report.diagnostic_index = reflector;
  return Status(ErrorCode::kNumerical);
}

void Complete(LapackReport& report) {
  report.outcome = LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
}

template <typename T>
bool MakeReflector(DenseBlasMatrixView<T> matrix, index_t column,
                   DenseBlasVectorView<T> tau) {
  using Real = DenseBlasRealType<T>;
  const T alpha = At(matrix, column, column);
  Real scale = ComponentMagnitude(alpha);
  bool nonzero_tail = false;
  for (index_t row = column + 1; row < matrix.rows(); ++row) {
    const T value = At(matrix, row, column);
    nonzero_tail = nonzero_tail || value != T{0};
    scale = std::max(scale, ComponentMagnitude(value));
  }
  if (!nonzero_tail && RealValue(alpha)) {
    tau.data()[column] = T{0};
    return true;
  }
  Real norm = 0;
  for (index_t row = column; row < matrix.rows(); ++row) {
    norm = NormWith(norm, At(matrix, row, column) / scale);
  }
  const Real normalized_beta = -std::copysign(norm, RealPart(alpha));
  const Real beta = normalized_beta * scale;
  if (!std::isfinite(beta) || beta == 0) {
    return false;
  }
  // Normalize before subtraction/division: alpha-beta may overflow even when
  // beta and every reflector coefficient are representable. Compute tau/v
  // from the normalized norm, not a rounded subnormal physical beta, so the
  // reflector remains unitary even when the diagonal has subnormal rounding.
  const T coefficient = T{1} - (alpha / scale) / normalized_beta;
  for (index_t row = column + 1; row < matrix.rows(); ++row) {
    At(matrix, row, column) =
        -((At(matrix, row, column) / scale) / normalized_beta) / coefficient;
  }
  At(matrix, column, column) = T{beta};
  tau.data()[column] = coefficient;
  return true;
}

template <typename T>
T VectorAt(DenseBlasMatrixView<const T> factors, index_t reflector,
           index_t row) {
  return row == reflector ? T{1} : At(factors, row, reflector);
}

template <typename T>
T& SideAt(DenseBlasMatrixView<T> matrix, DenseBlasSide side, index_t outer,
          index_t inner) {
  return side == DenseBlasSide::kLeft ? At(matrix, inner, outer)
                                      : At(matrix, outer, inner);
}

template <typename T>
DenseBlasRealType<T> SegmentScale(DenseBlasMatrixView<T> matrix,
                                  DenseBlasSide side, index_t outer,
                                  index_t first, extent_t order) {
  DenseBlasRealType<T> scale = 0;
  for (index_t inner = first; inner < order; ++inner) {
    scale =
        std::max(scale, ComponentMagnitude(SideAt(matrix, side, outer, inner)));
  }
  return scale;
}

template <typename T>
bool ApplyReflector(DenseBlasMatrixView<const T> factors, index_t reflector,
                    T tau, DenseBlasSide side, DenseBlasMatrixView<T> matrix,
                    index_t first_outer, DenseBlasVectorView<T> scratch) {
  if (tau == T{0}) {
    return true;
  }
  const extent_t order = factors.rows();
  const extent_t outer_count =
      side == DenseBlasSide::kLeft ? matrix.columns() : matrix.rows();
  for (index_t outer = first_outer; outer < outer_count; ++outer) {
    const auto scale = SegmentScale(matrix, side, outer, reflector, order);
    T dot{0};
    if (scale != 0) {
      for (index_t inner = reflector; inner < order; ++inner) {
        const T v = VectorAt(factors, reflector, inner);
        const T value = SideAt(matrix, side, outer, inner) / scale;
        dot += side == DenseBlasSide::kLeft ? Conjugate(v) * value : value * v;
      }
    }
    scratch.data()[outer] = tau * dot;
    if (!Finite(scratch.data()[outer])) {
      return false;
    }
  }
  for (index_t outer = first_outer; outer < outer_count; ++outer) {
    const auto scale = SegmentScale(matrix, side, outer, reflector, order);
    if (scale == 0) {
      continue;
    }
    for (index_t inner = reflector; inner < order; ++inner) {
      const T v = VectorAt(factors, reflector, inner);
      const T correction = side == DenseBlasSide::kLeft
                               ? v * scratch.data()[outer]
                               : scratch.data()[outer] * Conjugate(v);
      const T value =
          (SideAt(matrix, side, outer, inner) / scale - correction) * scale;
      if (!Finite(value)) {
        return false;
      }
      SideAt(matrix, side, outer, inner) = value;
    }
  }
  return true;
}

template <typename T>
Status GeqrfImpl(const ExecutionContext& context, DenseBlasMatrixView<T> matrix,
                 DenseBlasVectorView<T> tau, DenseBlasVectorView<T> scratch,
                 LapackReport& report, std::string_view name) {
  const std::array spans{matrix.reachable_storage(), tau.reachable_storage(),
                         scratch.reachable_storage()};
  auto status = Start<T>(
      name,
      std::array{matrix.rows(), matrix.columns(), tau.size(), scratch.size()},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension(), tau.increment(),
                                  scratch.increment()},
      spans, report);
  if (status.ok()) {
    status = ValidateExecution(context, spans);
  }
  if (!status.ok()) {
    return status;
  }
  const extent_t count = std::min(matrix.rows(), matrix.columns());
  const extent_t required = count == 0 ? 0 : matrix.columns();
  if (tau.size() != count || scratch.size() < required) {
    return Status(ErrorCode::kShape);
  }
  if (tau.increment() != 1 || scratch.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (count != 0) {
    status = ValidateFinite(matrix, report);
    if (!status.ok()) {
      return status;
    }
  }
  report.factor_family = LapackFactorFamily::kHouseholderQr;
  for (index_t column = 0; column < count; ++column) {
    if (!MakeReflector(matrix, column, tau) ||
        !ApplyReflector(DenseBlasMatrixView<const T>(matrix), column,
                        Conjugate(tau.data()[column]), DenseBlasSide::kLeft,
                        matrix, column + 1, scratch)) {
      return NumericalFailure(column, report);
    }
  }
  Complete(report);
  return Status::Ok();
}

template <typename T>
bool ValidTranspose(DenseBlasTranspose transpose) {
  if constexpr (DenseBlasComplex<T>) {
    return transpose == DenseBlasTranspose::kNone ||
           transpose == DenseBlasTranspose::kConjugateTranspose;
  } else {
    return transpose == DenseBlasTranspose::kNone ||
           transpose == DenseBlasTranspose::kTranspose;
  }
}

template <typename T>
Status PreflightFactor(const ExecutionContext& context,
                       LapackHouseholderQrFactorView<T> factor,
                       DenseBlasMatrixView<T> matrix,
                       DenseBlasVectorView<T> scratch, extent_t required) {
  auto status = ValidateExecution(
      context,
      std::array{factor.factors().reachable_storage(),
                 factor.tau().reachable_storage(), matrix.reachable_storage(),
                 scratch.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  if (factor.provider() != LapackProviderIdentity{}) {
    return Status(ErrorCode::kInvalidState);
  }
  if (scratch.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (scratch.size() < required) {
    return Status(ErrorCode::kShape);
  }
  return Status::Ok();
}

template <typename T>
Status ApplySequence(DenseBlasSide side, DenseBlasTranspose transpose,
                     LapackHouseholderQrFactorView<T> factor,
                     DenseBlasMatrixView<T> matrix,
                     DenseBlasVectorView<T> scratch, LapackReport& report) {
  const bool normal = transpose == DenseBlasTranspose::kNone;
  const bool forward = (side == DenseBlasSide::kRight) == normal;
  const auto count = factor.tau().size();
  for (index_t step = 0; step < count; ++step) {
    const index_t i = forward ? step : count - 1 - step;
    const T tau =
        normal ? factor.tau().data()[i] : Conjugate(factor.tau().data()[i]);
    if (!ApplyReflector(factor.factors(), i, tau, side, matrix, 0, scratch)) {
      return NumericalFailure(i, report);
    }
  }
  return Status::Ok();
}

template <typename T>
Status StartFactor(std::string_view name,
                   LapackHouseholderQrFactorView<T> factor,
                   DenseBlasMatrixView<T> matrix,
                   DenseBlasVectorView<T> scratch, DenseBlasSide side,
                   DenseBlasTranspose transpose, LapackReport& report) {
  return Start<T>(
      name,
      std::array{factor.factors().rows(), factor.factors().columns(),
                 factor.tau().size(), matrix.rows(), matrix.columns(),
                 scratch.size()},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(factor.factors().layout()),
          factor.factors().leading_dimension(),
          static_cast<std::int64_t>(matrix.layout()),
          matrix.leading_dimension(), static_cast<std::int64_t>(side),
          static_cast<std::int64_t>(transpose), scratch.increment()},
      std::array{factor.factors().reachable_storage(),
                 factor.tau().reachable_storage(), matrix.reachable_storage(),
                 scratch.reachable_storage()},
      report);
}

template <typename T>
Status FormImpl(const ExecutionContext& context,
                LapackHouseholderQrFactorView<T> factor,
                DenseBlasMatrixView<T> q, DenseBlasVectorView<T> scratch,
                LapackReport& report) {
  auto status =
      StartFactor("form_householder_q", factor, q, scratch,
                  DenseBlasSide::kLeft, DenseBlasTranspose::kNone, report);
  const bool active =
      q.rows() != 0 && q.columns() != 0 && factor.tau().size() != 0;
  if (status.ok()) {
    status =
        PreflightFactor(context, factor, q, scratch, active ? q.columns() : 0);
  }
  if (!status.ok()) {
    return status;
  }
  if (q.rows() != factor.factors().rows() ||
      (q.columns() != q.rows() && q.columns() != factor.tau().size())) {
    return Status(ErrorCode::kShape);
  }
  if (active) {
    status = ValidateReflectors(factor, report);
    if (!status.ok()) {
      return status;
    }
  }
  for (index_t column = 0; column < q.columns(); ++column) {
    for (index_t row = 0; row < q.rows(); ++row) {
      At(q, row, column) = row == column ? T{1} : T{0};
    }
  }
  if (active) {
    status = ApplySequence(DenseBlasSide::kLeft, DenseBlasTranspose::kNone,
                           factor, q, scratch, report);
    if (!status.ok()) {
      return status;
    }
  }
  Complete(report);
  return Status::Ok();
}

template <typename T>
Status ApplyImpl(const ExecutionContext& context, DenseBlasSide side,
                 DenseBlasTranspose transpose,
                 LapackHouseholderQrFactorView<T> factor,
                 DenseBlasMatrixView<T> matrix, DenseBlasVectorView<T> scratch,
                 LapackReport& report) {
  auto status = StartFactor("apply_householder_q", factor, matrix, scratch,
                            side, transpose, report);
  const bool active =
      matrix.rows() != 0 && matrix.columns() != 0 && factor.tau().size() != 0;
  extent_t required = 0;
  if (active) {
    required = side == DenseBlasSide::kLeft ? matrix.columns() : matrix.rows();
  }
  if (status.ok()) {
    status = PreflightFactor(context, factor, matrix, scratch, required);
  }
  if (!status.ok()) {
    return status;
  }
  if ((side != DenseBlasSide::kLeft && side != DenseBlasSide::kRight) ||
      !ValidTranspose<T>(transpose)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto order =
      side == DenseBlasSide::kLeft ? matrix.rows() : matrix.columns();
  if (order != factor.factors().rows()) {
    return Status(ErrorCode::kShape);
  }
  if (active) {
    status = ValidateReflectors(factor, report);
    if (status.ok()) {
      status = ValidateFinite(matrix, report);
    }
    if (!status.ok()) {
      return status;
    }
    status = ApplySequence(side, transpose, factor, matrix, scratch, report);
    if (!status.ok()) {
      return status;
    }
  }
  Complete(report);
  return Status::Ok();
}

}  // namespace

Status Geqrf(const ExecutionContext& context, DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<float> tau, DenseBlasVectorView<float> scratch,
             LapackReport& report) {
  return GeqrfImpl(context, matrix, tau, scratch, report, "sgeqrf");
}
Status Geqrf(const ExecutionContext& context,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<double> tau,
             DenseBlasVectorView<double> scratch, LapackReport& report) {
  return GeqrfImpl(context, matrix, tau, scratch, report, "dgeqrf");
}
Status Geqrf(const ExecutionContext& context,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<std::complex<float>> tau,
             DenseBlasVectorView<std::complex<float>> scratch,
             LapackReport& report) {
  return GeqrfImpl(context, matrix, tau, scratch, report, "cgeqrf");
}
Status Geqrf(const ExecutionContext& context,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<std::complex<double>> tau,
             DenseBlasVectorView<std::complex<double>> scratch,
             LapackReport& report) {
  return GeqrfImpl(context, matrix, tau, scratch, report, "zgeqrf");
}
Status FormHouseholderQ(const ExecutionContext& context,
                        LapackHouseholderQrFactorView<float> factor,
                        DenseBlasMatrixView<float> q,
                        DenseBlasVectorView<float> scratch,
                        LapackReport& report) {
  return FormImpl(context, factor, q, scratch, report);
}
Status FormHouseholderQ(const ExecutionContext& context,
                        LapackHouseholderQrFactorView<double> factor,
                        DenseBlasMatrixView<double> q,
                        DenseBlasVectorView<double> scratch,
                        LapackReport& report) {
  return FormImpl(context, factor, q, scratch, report);
}
Status FormHouseholderQ(
    const ExecutionContext& context,
    LapackHouseholderQrFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> q,
    DenseBlasVectorView<std::complex<float>> scratch, LapackReport& report) {
  return FormImpl(context, factor, q, scratch, report);
}
Status FormHouseholderQ(
    const ExecutionContext& context,
    LapackHouseholderQrFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> q,
    DenseBlasVectorView<std::complex<double>> scratch, LapackReport& report) {
  return FormImpl(context, factor, q, scratch, report);
}
Status ApplyHouseholderQ(const ExecutionContext& context, DenseBlasSide side,
                         DenseBlasTranspose transpose,
                         LapackHouseholderQrFactorView<float> factor,
                         DenseBlasMatrixView<float> matrix,
                         DenseBlasVectorView<float> scratch,
                         LapackReport& report) {
  return ApplyImpl(context, side, transpose, factor, matrix, scratch, report);
}
Status ApplyHouseholderQ(const ExecutionContext& context, DenseBlasSide side,
                         DenseBlasTranspose transpose,
                         LapackHouseholderQrFactorView<double> factor,
                         DenseBlasMatrixView<double> matrix,
                         DenseBlasVectorView<double> scratch,
                         LapackReport& report) {
  return ApplyImpl(context, side, transpose, factor, matrix, scratch, report);
}
Status ApplyHouseholderQ(
    const ExecutionContext& context, DenseBlasSide side,
    DenseBlasTranspose transpose,
    LapackHouseholderQrFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> scratch, LapackReport& report) {
  return ApplyImpl(context, side, transpose, factor, matrix, scratch, report);
}
Status ApplyHouseholderQ(
    const ExecutionContext& context, DenseBlasSide side,
    DenseBlasTranspose transpose,
    LapackHouseholderQrFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> scratch, LapackReport& report) {
  return ApplyImpl(context, side, transpose, factor, matrix, scratch, report);
}

}  // namespace asc
