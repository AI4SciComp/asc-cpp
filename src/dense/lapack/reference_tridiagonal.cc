#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "internal_layout.h"
#include "internal_tridiagonal.h"
#include "internal_tridiagonal_counts.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr std::string_view kFactor = "sgttrf";
  static constexpr std::string_view kSolve = "sgttrs";
  static lapack_int Factor(lapack_int n, float* lower, float* diagonal,
                           float* upper, float* second, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgttrf(&n, lower, diagonal, upper, second, pivots, &info);
    return info;
  }
  static lapack_int Solve(char transpose, lapack_int n, lapack_int nrhs,
                          const float* lower, const float* diagonal,
                          const float* upper, const float* second,
                          const lapack_int* pivots, float* rhs,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgttrs(&transpose, &n, &nrhs, lower, diagonal, upper, second, pivots,
                  rhs, &ldb, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kFactor = "dgttrf";
  static constexpr std::string_view kSolve = "dgttrs";
  static lapack_int Factor(lapack_int n, double* lower, double* diagonal,
                           double* upper, double* second, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgttrf(&n, lower, diagonal, upper, second, pivots, &info);
    return info;
  }
  static lapack_int Solve(char transpose, lapack_int n, lapack_int nrhs,
                          const double* lower, const double* diagonal,
                          const double* upper, const double* second,
                          const lapack_int* pivots, double* rhs,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgttrs(&transpose, &n, &nrhs, lower, diagonal, upper, second, pivots,
                  rhs, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kFactor = "cgttrf";
  static constexpr std::string_view kSolve = "cgttrs";
  static lapack_int Factor(lapack_int n, std::complex<float>* lower,
                           std::complex<float>* diagonal,
                           std::complex<float>* upper,
                           std::complex<float>* second, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgttrf(&n, lower, diagonal, upper, second, pivots, &info);
    return info;
  }
  static lapack_int Solve(char transpose, lapack_int n, lapack_int nrhs,
                          const std::complex<float>* lower,
                          const std::complex<float>* diagonal,
                          const std::complex<float>* upper,
                          const std::complex<float>* second,
                          const lapack_int* pivots, std::complex<float>* rhs,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgttrs(&transpose, &n, &nrhs, lower, diagonal, upper, second, pivots,
                  rhs, &ldb, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kFactor = "zgttrf";
  static constexpr std::string_view kSolve = "zgttrs";
  static lapack_int Factor(lapack_int n, std::complex<double>* lower,
                           std::complex<double>* diagonal,
                           std::complex<double>* upper,
                           std::complex<double>* second, lapack_int* pivots) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgttrf(&n, lower, diagonal, upper, second, pivots, &info);
    return info;
  }
  static lapack_int Solve(char transpose, lapack_int n, lapack_int nrhs,
                          const std::complex<double>* lower,
                          const std::complex<double>* diagonal,
                          const std::complex<double>* upper,
                          const std::complex<double>* second,
                          const lapack_int* pivots, std::complex<double>* rhs,
                          lapack_int ldb) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgttrs(&transpose, &n, &nrhs, lower, diagonal, upper, second, pivots,
                  rhs, &ldb, &info);
    return info;
  }
};

template <typename T>
auto FactorOperands(LapackTridiagonalLuStorage<T> factors,
                    DenseBlasVectorView<index_t> pivots) {
  const auto spans = checked::Spans(factors);
  return std::array{spans[0], spans[1], spans[2], spans[3],
                    pivots.reachable_storage()};
}

template <typename T>
auto SolveOperands(ReferenceTridiagonalLuFactorView<T> factor,
                   DenseBlasMatrixView<T> rhs) {
  const auto spans = checked::Spans(factor.factors());
  return std::array{spans[0],
                    spans[1],
                    spans[2],
                    spans[3],
                    factor.pivots().reachable_storage(),
                    rhs.reachable_storage()};
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        LapackTridiagonalLuStorage<T> factors,
                                        DenseBlasVectorView<index_t> pivots) {
  const extent_t order = factors.primary().order();
  Status status = checked::Storage(provider, factors);
  if (!status.ok()) {
    return status;
  }
  status = checked::Vector(provider, pivots, order);
  if (!status.ok()) {
    return status;
  }
  status = checked::Disjoint(FactorOperands(factors, pivots));
  if (!status.ok()) {
    return status;
  }
  status = internal_tridiagonal_counts::Factor(order, checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kFactor, checked::Kind<T>(), std::array{order},
      std::span<const std::int64_t>{}, provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = checked::AddPivots(order, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
Status Factor(const ReferenceLapackProvider& provider,
              LapackTridiagonalLuStorage<T> factors,
              DenseBlasVectorView<index_t> pivots,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const auto operands = FactorOperands(factors, pivots);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kFactor, report);
  const auto expected = QueryFactor(provider, factors, pivots);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t order = factors.primary().order();
  if (order == 0) {
    return checked::Complete(report);
  }
  auto* native_pivots = checked::OutputPivotObjects(order, workspace);
  T lower_dummy{};
  T upper_dummy{};
  T second_dummy{};
  report.called_provider = true;
  const auto info = Native<T>::Factor(
      static_cast<lapack_int>(order),
      checked::Nonnull(factors.primary().lower().data(), lower_dummy),
      factors.primary().diagonal().data(),
      checked::Nonnull(factors.primary().upper().data(), upper_dummy),
      checked::Nonnull(factors.second_upper().data(), second_dummy),
      native_pivots);
  report.native_info = info;
  if (info < 0 || info > order ||
      !checked::NativePivotValues(native_pivots, order).ok()) {
    return checked::ProviderDefect(info, report);
  }
  for (extent_t i = 0; i < order; ++i) {
    pivots.data()[i] = native_pivots[i];
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<T> factor, DenseBlasMatrixView<T> rhs) {
  Status status = checked::Transpose(transpose);
  if (!status.ok()) {
    return status;
  }
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  const auto factors = factor.factors();
  const extent_t order = factors.primary().order();
  status = checked::Storage(provider, factors);
  if (!status.ok()) {
    return status;
  }
  status = checked::PivotMetadata(provider, factor.pivots(), order);
  if (!status.ok()) {
    return status;
  }
  status = checked::Matrix(provider, rhs, order);
  if (!status.ok()) {
    return status;
  }
  status = checked::Disjoint(SolveOperands(factor, rhs));
  if (!status.ok()) {
    return status;
  }
  status =
      internal_tridiagonal_counts::Solve(order, rhs.columns(), checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kSolve, checked::Kind<T>(),
      std::array{order, rhs.columns(), checked::Leading(rhs)},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(transpose),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (order != 0 && rhs.columns() != 0) {
    status = checked::AddPivots(order, plan);
    if (status.ok()) {
      status = internal_lapack_layout::AddPacking(rhs, plan);
    }
  }
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceTridiagonalLuFactorView<T> factor,
             DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = SolveOperands(factor, rhs);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kSolve, report);
  const auto expected = QuerySolve(provider, transpose, factor, rhs);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const auto factors = factor.factors();
  const extent_t order = factors.primary().order();
  if (order == 0 || rhs.columns() == 0) {
    return checked::Complete(report);
  }
  status = checked::PivotValues(factor.pivots());
  if (!status.ok()) {
    return status;
  }
  status = checked::ZeroDiagonal(factors, report);
  if (!status.ok()) {
    return status;
  }
  const auto* native_pivots =
      checked::ConvertPivots(factor.pivots(), workspace);
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  T* packed = internal_lapack_layout::Pack(rhs, cursor);
  const T lower_dummy{};
  const T upper_dummy{};
  const T second_dummy{};
  report.called_provider = true;
  const auto info = Native<T>::Solve(
      checked::TransposeCharacter(transpose), static_cast<lapack_int>(order),
      static_cast<lapack_int>(rhs.columns()),
      checked::Nonnull<const T>(factors.primary().lower().data(), lower_dummy),
      factors.primary().diagonal().data(),
      checked::Nonnull<const T>(factors.primary().upper().data(), upper_dummy),
      checked::Nonnull<const T>(factors.second_upper().data(), second_dummy),
      native_pivots, packed, static_cast<lapack_int>(checked::Leading(rhs)));
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  internal_lapack_layout::Unpack(packed, rhs);
  return checked::Complete(report);
}
}  // namespace

Result<ReferenceTridiagonalPivotView> ReferenceTridiagonalPivotView::Create(
    DenseBlasVectorView<const index_t> values) {
  if (values.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  if (values.memory_space() != MemorySpace::kHost &&
      values.memory_space() != MemorySpace::kPinnedHost) {
    return Status(ErrorCode::kMemoryAccess);
  }
  return ReferenceTridiagonalPivotView(values);
}

template <DenseBlasScalar Element>
Result<ReferenceTridiagonalLuFactorView<Element>>
ReferenceTridiagonalLuFactorView<Element>::Create(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<const Element> factors,
    ReferenceTridiagonalPivotView pivots, const LapackReport& report) {
  const auto spans = checked::Spans(factors);
  const std::array operands{spans[0],
                            spans[1],
                            spans[2],
                            spans[3],
                            pivots.reachable_storage(),
                            checked::ObjectStorage(provider),
                            checked::ObjectStorage(report)};
  Status status = checked::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  status = checked::Storage(provider, factors);
  if (!status.ok()) {
    return status;
  }
  const extent_t order = factors.primary().order();
  status = checked::PivotMetadata(provider, pivots, order);
  if (!status.ok()) {
    return status;
  }
  const auto name = Native<Element>::kFactor;
  const bool actual_call = report.called_provider && report.native_info == 0;
  const bool empty_call =
      !report.called_provider && !report.native_info.has_value();
  if (report.provider != provider.identity() ||
      report.outcome != LapackOutcome::kSuccess ||
      report.output_validity != LapackOutputValidity::kComplete ||
      report.factor_family.has_value() ||
      std::string_view(report.routine.data(), name.size()) != name ||
      report.routine[name.size()] != '\0' ||
      (order == 0 ? !empty_call : !actual_call)) {
    return Status(ErrorCode::kInvalidState);
  }
  status = internal_tridiagonal_counts::Factor(order, checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  status = checked::PivotValues(pivots);
  if (!status.ok()) {
    return status;
  }
  LapackReport validation;
  status = checked::ZeroDiagonal(factors, validation);
  if (!status.ok()) {
    return status;
  }
  return ReferenceTridiagonalLuFactorView(factors, pivots, report.provider);
}

template class ReferenceTridiagonalLuFactorView<float>;

Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<float> factors,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, factors, pivots);
}
Status Gttrf(const ReferenceLapackProvider& provider,
             LapackTridiagonalLuStorage<float> factors,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<float> factor,
    DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Gttrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceTridiagonalLuFactorView<float> factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}

template class ReferenceTridiagonalLuFactorView<double>;

Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<double> factors,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, factors, pivots);
}
Status Gttrf(const ReferenceLapackProvider& provider,
             LapackTridiagonalLuStorage<double> factors,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<double> factor,
    DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Gttrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceTridiagonalLuFactorView<double> factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}

template class ReferenceTridiagonalLuFactorView<std::complex<float>>;

Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, factors, pivots);
}
Status Gttrf(const ReferenceLapackProvider& provider,
             LapackTridiagonalLuStorage<std::complex<float>> factors,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Gttrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceTridiagonalLuFactorView<std::complex<float>> factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}

template class ReferenceTridiagonalLuFactorView<std::complex<double>>;

Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, factors, pivots);
}
Status Gttrf(const ReferenceLapackProvider& provider,
             LapackTridiagonalLuStorage<std::complex<double>> factors,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, factors, pivots, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, transpose, factor, rhs);
}
Status Gttrs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             ReferenceTridiagonalLuFactorView<std::complex<double>> factor,
             DenseBlasMatrixView<std::complex<double>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, transpose, factor, rhs, plan, workspace, report);
}

}  // namespace asc
