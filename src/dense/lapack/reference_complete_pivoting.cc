#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_complete_pivoting.h"
#include "internal_complete_pivoting_counts.h"
#include "internal_complete_pivoting_prototypes.h"
#include "internal_layout.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <bool Solve, typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return Solve ? "sgesc2" : "sgetc2";
  } else if constexpr (std::is_same_v<T, double>) {
    return Solve ? "dgesc2" : "dgetc2";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Solve ? "cgesc2" : "cgetc2";
  } else {
    return Solve ? "zgesc2" : "zgetc2";
  }
}

template <typename T>
extent_t Offset(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? j * matrix.leading_dimension() + i
             : i * matrix.leading_dimension() + j;
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
bool FiniteMatrix(DenseBlasMatrixView<T> matrix) {
  for (extent_t j = 0; j < matrix.columns(); ++j) {
    for (extent_t i = 0; i < matrix.rows(); ++i) {
      if (!Finite(matrix.data()[Offset(matrix, i, j)])) {
        return false;
      }
    }
  }
  return true;
}

Status CheckSpans(const ReferenceLapackProvider& provider,
                  std::span<const ConstMemoryView> spans) {
  Status status = checked::Disjoint(spans);
  for (const auto span : spans) {
    if (status.ok()) {
      status = checked::Access(provider, span);
    }
  }
  return status;
}

template <typename T>
Result<LapackWorkspacePlan> Plan(const LapackPlanIdentity& identity, extent_t n,
                                 bool solve) {
  const auto counts =
      internal_complete_pivoting_counts::Count(n, solve, checked::kLimit);
  if (!counts.ok()) {
    return counts.status();
  }
  LapackWorkspacePlan plan{identity};
  if (n != 0) {
    plan.regions[checked::kScratch] = {counts->scalar, counts->scalar,
                                       sizeof(T), alignof(T)};
    plan.regions[checked::kInteger] = {counts->integer, counts->integer,
                                       sizeof(lapack_int), alignof(lapack_int)};
  }
  const Status status = internal_lapack_layout::CheckTotal(plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        DenseBlasMatrixView<T> matrix,
                                        DenseBlasVectorView<index_t> rows,
                                        DenseBlasVectorView<index_t> columns) {
  const extent_t n = matrix.rows();
  if (matrix.columns() != n || rows.size() != n || columns.size() != n ||
      rows.increment() != 1 || columns.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  const Status status = CheckSpans(
      provider, std::array{matrix.reachable_storage(), rows.reachable_storage(),
                           columns.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<false, T>(), checked::Kind<T>(),
      std::array{n, matrix.leading_dimension(), rows.size(), columns.size()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(matrix.layout()),
                                  rows.increment(), columns.increment()},
      provider.identity());
  return identity.ok() ? Plan<T>(*identity, n, false)
                       : Result<LapackWorkspacePlan>(identity.status());
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<T> factor, DenseBlasVectorView<T> rhs,
    DenseBlasVectorView<DenseBlasRealType<T>> scale) {
  const auto matrix = factor.factors();
  const auto rows = factor.row_pivots();
  const auto columns = factor.column_pivots();
  const extent_t n = matrix.rows();
  if (factor.provider() != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  if (rhs.size() != n || rhs.increment() != 1 || scale.size() != 1 ||
      scale.increment() != 1) {
    return Status(ErrorCode::kShape);
  }
  const Status status = CheckSpans(
      provider, std::array{matrix.reachable_storage(), rows.reachable_storage(),
                           columns.reachable_storage(), rhs.reachable_storage(),
                           scale.reachable_storage()});
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<true, T>(), checked::Kind<T>(),
      std::array{n, matrix.leading_dimension(), rows.size(), columns.size(),
                 rhs.size(), scale.size()},
      std::array<std::int64_t, 6>{static_cast<std::int64_t>(matrix.layout()),
                                  rows.increment(), columns.increment(),
                                  rhs.increment(), scale.increment(),
                                  factor.perturbed()},
      provider.identity());
  return identity.ok() ? Plan<T>(*identity, n, true)
                       : Result<LapackWorkspacePlan>(identity.status());
}

template <typename T>
void Pack(DenseBlasMatrixView<T> matrix, std::remove_const_t<T>* output) {
  const extent_t n = matrix.rows();
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < n; ++i) {
      output[j * n + i] = matrix.data()[Offset(matrix, i, j)];
    }
  }
}

template <typename T>
void Publish(const T* input, DenseBlasMatrixView<T> matrix) {
  const extent_t n = matrix.rows();
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < n; ++i) {
      matrix.data()[Offset(matrix, i, j)] = input[j * n + i];
    }
  }
}

template <typename T>
lapack_int NativeFactor(lapack_int n, T* a, lapack_int* rows,
                        lapack_int* columns) {
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_sgetc2(&n, a, &n, rows, columns, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    ASC_LAPACK_dgetc2(&n, a, &n, rows, columns, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    ASC_LAPACK_cgetc2(&n, a, &n, rows, columns, &info);
  } else {
    ASC_LAPACK_zgetc2(&n, a, &n, rows, columns, &info);
  }
  return info;
}

template <typename T>
DenseBlasRealType<T> NativeSolve(lapack_int n, const T* a, T* rhs,
                                 const lapack_int* rows,
                                 const lapack_int* columns) {
  auto scale = std::numeric_limits<DenseBlasRealType<T>>::quiet_NaN();
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_sgesc2(&n, a, &n, rhs, rows, columns, &scale);
  } else if constexpr (std::is_same_v<T, double>) {
    ASC_LAPACK_dgesc2(&n, a, &n, rhs, rows, columns, &scale);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    ASC_LAPACK_cgesc2(&n, a, &n, rhs, rows, columns, &scale);
  } else {
    ASC_LAPACK_zgesc2(&n, a, &n, rhs, rows, columns, &scale);
  }
  return scale;
}

Status Complete(bool warning, LapackReport& report) {
  report.outcome =
      warning ? LapackOutcome::kAccuracyWarning : LapackOutcome::kSuccess;
  report.output_validity = LapackOutputValidity::kComplete;
  return Status::Ok();
}

Status Unusable(ErrorCode code, LapackReport& report) {
  report.outcome = LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kUnusable;
  return Status(code);
}

template <typename T>
Status Factor(const ReferenceLapackProvider& provider,
              DenseBlasMatrixView<T> matrix, DenseBlasVectorView<index_t> rows,
              DenseBlasVectorView<index_t> columns,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  const std::array spans{matrix.reachable_storage(), rows.reachable_storage(),
                         columns.reachable_storage()};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<false, T>(), report);
  const auto expected = QueryFactor(provider, matrix, rows, columns);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  const extent_t n = matrix.rows();
  if (n == 0) {
    return checked::Complete(report);
  }
  if (!FiniteMatrix(matrix)) {
    return Status(ErrorCode::kNumerical);
  }
  auto* staged = static_cast<T*>(workspace.regions[checked::kScratch].data());
  Pack(matrix, staged);
  auto* pivots = checked::IntegerObjects(checked::kInteger, 2 * n, workspace);
  std::fill_n(pivots, 2 * n, std::numeric_limits<lapack_int>::min());
  report.called_provider = true;
  const auto info =
      NativeFactor(static_cast<lapack_int>(n), staged, pivots, pivots + n);
  report.native_info = info;
  if (info < 0 || info > n) {
    return checked::ProviderDefect(info, report);
  }
  for (extent_t i = 0; i < n; ++i) {
    if (pivots[i] < i + 1 || pivots[i] > n || pivots[n + i] < i + 1 ||
        pivots[n + i] > n) {
      return Unusable(ErrorCode::kProvider, report);
    }
  }
  for (extent_t i = 0; i < n * n; ++i) {
    if (!Finite(staged[i])) {
      return Unusable(ErrorCode::kNumerical, report);
    }
  }
  Publish(staged, matrix);
  for (extent_t i = 0; i < n; ++i) {
    rows.data()[i] = pivots[i];
    columns.data()[i] = pivots[n + i];
  }
  if (info > 0) {
    report.diagnostic_index = static_cast<index_t>(info) - 1;
  }
  return Complete(info > 0, report);
}

template <typename T>
Status SolveValues(ReferenceCompletePivotLuFactorView<T> factor,
                   DenseBlasVectorView<T> rhs, LapackReport& report) {
  const auto matrix = factor.factors();
  const extent_t n = matrix.rows();
  for (extent_t i = 0; i < n; ++i) {
    const auto row = factor.row_pivots().data()[i];
    const auto column = factor.column_pivots().data()[i];
    if (row < i + 1 || row > n || column < i + 1 || column > n) {
      return Status(ErrorCode::kIndex);
    }
  }
  if (!FiniteMatrix(matrix)) {
    return Status(ErrorCode::kNumerical);
  }
  for (extent_t i = 0; i < n; ++i) {
    if (!Finite(rhs.data()[i])) {
      return Status(ErrorCode::kNumerical);
    }
    if (matrix.data()[Offset(matrix, i, i)] == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = i;
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename T>
bool Preserved(ReferenceCompletePivotLuFactorView<T> factor, const T* staged,
               const lapack_int* pivots) {
  const auto matrix = factor.factors();
  const extent_t n = matrix.rows();
  for (extent_t j = 0; j < n; ++j) {
    if (pivots[j] != factor.row_pivots().data()[j] ||
        pivots[n + j] != factor.column_pivots().data()[j]) {
      return false;
    }
    for (extent_t i = 0; i < n; ++i) {
      // Exact preservation includes signed zero; numerical equality is weaker.
      const auto actual = std::as_bytes(std::span(staged + j * n + i, 1));
      const auto original =
          std::as_bytes(std::span(matrix.data() + Offset(matrix, i, j), 1));
      if (!std::equal(actual.begin(), actual.end(), original.begin())) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
Status Solve(const ReferenceLapackProvider& provider,
             ReferenceCompletePivotLuFactorView<T> factor,
             DenseBlasVectorView<T> rhs,
             DenseBlasVectorView<DenseBlasRealType<T>> scale,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  const auto matrix = factor.factors();
  const extent_t n = matrix.rows();
  const std::array spans{matrix.reachable_storage(),
                         factor.row_pivots().reachable_storage(),
                         factor.column_pivots().reachable_storage(),
                         rhs.reachable_storage(), scale.reachable_storage()};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<true, T>(), report);
  const auto expected = QuerySolve(provider, factor, rhs, scale);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  if (n == 0) {
    scale.data()[0] = DenseBlasRealType<T>{1};
    return checked::Complete(report);
  }
  status = SolveValues(factor, rhs, report);
  if (!status.ok()) {
    return status;
  }
  auto* staged = static_cast<T*>(workspace.regions[checked::kScratch].data());
  Pack(matrix, staged);
  std::copy_n(rhs.data(), n, staged + n * n);
  auto* pivots = checked::IntegerObjects(checked::kInteger, 2 * n, workspace);
  for (extent_t i = 0; i < n; ++i) {
    pivots[i] = static_cast<lapack_int>(factor.row_pivots().data()[i]);
    pivots[n + i] = static_cast<lapack_int>(factor.column_pivots().data()[i]);
  }
  report.called_provider = true;
  const auto result = NativeSolve(static_cast<lapack_int>(n), staged,
                                  staged + n * n, pivots, pivots + n);
  if (!std::isfinite(result) || result < 0 || result > 1 ||
      !Preserved(factor, staged, pivots)) {
    return Unusable(ErrorCode::kProvider, report);
  }
  for (extent_t i = 0; i < n; ++i) {
    if (!Finite(staged[n * n + i])) {
      return Unusable(ErrorCode::kNumerical, report);
    }
  }
  std::copy_n(staged + n * n, n, rhs.data());
  scale.data()[0] = result;
  return Complete(factor.perturbed() || result < 1, report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots) {
  return QueryFactor(provider, matrix, row_pivots, column_pivots);
}
Status Getc2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> row_pivots,
             DenseBlasVectorView<index_t> column_pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, row_pivots, column_pivots, plan, workspace,
                report);
}
Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<float> factor,
    DenseBlasVectorView<float> rhs, DenseBlasVectorView<float> scale) {
  return QuerySolve(provider, factor, rhs, scale);
}
Status Gesc2(const ReferenceLapackProvider& provider,
             ReferenceCompletePivotLuFactorView<float> factor,
             DenseBlasVectorView<float> rhs, DenseBlasVectorView<float> scale,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, factor, rhs, scale, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots) {
  return QueryFactor(provider, matrix, row_pivots, column_pivots);
}
Status Getc2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> row_pivots,
             DenseBlasVectorView<index_t> column_pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, row_pivots, column_pivots, plan, workspace,
                report);
}
Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<double> factor,
    DenseBlasVectorView<double> rhs, DenseBlasVectorView<double> scale) {
  return QuerySolve(provider, factor, rhs, scale);
}
Status Gesc2(const ReferenceLapackProvider& provider,
             ReferenceCompletePivotLuFactorView<double> factor,
             DenseBlasVectorView<double> rhs, DenseBlasVectorView<double> scale,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Solve(provider, factor, rhs, scale, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots) {
  return QueryFactor(provider, matrix, row_pivots, column_pivots);
}
Status Getc2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> row_pivots,
             DenseBlasVectorView<index_t> column_pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, row_pivots, column_pivots, plan, workspace,
                report);
}
Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<std::complex<float>> factor,
    DenseBlasVectorView<std::complex<float>> rhs,
    DenseBlasVectorView<float> scale) {
  return QuerySolve(provider, factor, rhs, scale);
}
Status Gesc2(const ReferenceLapackProvider& provider,
             ReferenceCompletePivotLuFactorView<std::complex<float>> factor,
             DenseBlasVectorView<std::complex<float>> rhs,
             DenseBlasVectorView<float> scale, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, factor, rhs, scale, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots) {
  return QueryFactor(provider, matrix, row_pivots, column_pivots);
}
Status Getc2(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> row_pivots,
             DenseBlasVectorView<index_t> column_pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Factor(provider, matrix, row_pivots, column_pivots, plan, workspace,
                report);
}
Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<std::complex<double>> factor,
    DenseBlasVectorView<std::complex<double>> rhs,
    DenseBlasVectorView<double> scale) {
  return QuerySolve(provider, factor, rhs, scale);
}
Status Gesc2(const ReferenceLapackProvider& provider,
             ReferenceCompletePivotLuFactorView<std::complex<double>> factor,
             DenseBlasVectorView<std::complex<double>> rhs,
             DenseBlasVectorView<double> scale, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Solve(provider, factor, rhs, scale, plan, workspace, report);
}
}  // namespace asc
