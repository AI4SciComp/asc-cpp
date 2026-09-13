#include <array>
#include <complex>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "internal_layout.h"
#include "internal_tridiagonal.h"
#include "internal_tridiagonal_expert.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr std::string_view kName = "sgtsvx";
  static lapack_int Execute(char fact, char transpose, lapack_int n,
                            lapack_int nrhs, const float* lower,
                            const float* diagonal, const float* upper,
                            float* factor_lower, float* factor_diagonal,
                            float* factor_upper, float* second,
                            lapack_int* pivots, const float* rhs,
                            lapack_int ldb, float* solution, lapack_int ldx,
                            float& rcond, float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgtsvx(
        &fact, &transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
        factor_diagonal, factor_upper, second, pivots, rhs, &ldb, solution,
        &ldx, &rcond, ferr, berr,
        static_cast<float*>(workspace.regions[checked::kScalar].data()),
        checked::EstimatorIntegerObjects(n, workspace), &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kName = "dgtsvx";
  static lapack_int Execute(char fact, char transpose, lapack_int n,
                            lapack_int nrhs, const double* lower,
                            const double* diagonal, const double* upper,
                            double* factor_lower, double* factor_diagonal,
                            double* factor_upper, double* second,
                            lapack_int* pivots, const double* rhs,
                            lapack_int ldb, double* solution, lapack_int ldx,
                            double& rcond, double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgtsvx(
        &fact, &transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
        factor_diagonal, factor_upper, second, pivots, rhs, &ldb, solution,
        &ldx, &rcond, ferr, berr,
        static_cast<double*>(workspace.regions[checked::kScalar].data()),
        checked::EstimatorIntegerObjects(n, workspace), &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kName = "cgtsvx";
  static lapack_int Execute(
      char fact, char transpose, lapack_int n, lapack_int nrhs,
      const std::complex<float>* lower, const std::complex<float>* diagonal,
      const std::complex<float>* upper, std::complex<float>* factor_lower,
      std::complex<float>* factor_diagonal, std::complex<float>* factor_upper,
      std::complex<float>* second, lapack_int* pivots,
      const std::complex<float>* rhs, lapack_int ldb,
      std::complex<float>* solution, lapack_int ldx, float& rcond, float* ferr,
      float* berr, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgtsvx(&fact, &transpose, &n, &nrhs, lower, diagonal, upper,
                  factor_lower, factor_diagonal, factor_upper, second, pivots,
                  rhs, &ldb, solution, &ldx, &rcond, ferr, berr,
                  static_cast<std::complex<float>*>(
                      workspace.regions[checked::kScalar].data()),
                  static_cast<float*>(workspace.regions[checked::kReal].data()),
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kName = "zgtsvx";
  static lapack_int Execute(
      char fact, char transpose, lapack_int n, lapack_int nrhs,
      const std::complex<double>* lower, const std::complex<double>* diagonal,
      const std::complex<double>* upper, std::complex<double>* factor_lower,
      std::complex<double>* factor_diagonal, std::complex<double>* factor_upper,
      std::complex<double>* second, lapack_int* pivots,
      const std::complex<double>* rhs, lapack_int ldb,
      std::complex<double>* solution, lapack_int ldx, double& rcond,
      double* ferr, double* berr, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgtsvx(
        &fact, &transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
        factor_diagonal, factor_upper, second, pivots, rhs, &ldb, solution,
        &ldx, &rcond, ferr, berr,
        static_cast<std::complex<double>*>(
            workspace.regions[checked::kScalar].data()),
        static_cast<double*>(workspace.regions[checked::kReal].data()), &info);
    return info;
  }
};

template <typename T, typename FactorElement, typename Pivot>
auto Operands(LapackTridiagonalView<const T> original,
              LapackTridiagonalLuStorage<FactorElement> factors, Pivot pivots,
              DenseBlasMatrixView<const T> rhs, DenseBlasMatrixView<T> solution,
              const DenseBlasRealType<T>& reciprocal_condition,
              DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
              DenseBlasVectorView<DenseBlasRealType<T>> backward_error) {
  const auto common = checked::ExpertOperands(
      original, factors, pivots, rhs, solution, forward_error, backward_error);
  return std::array{common[0],
                    common[1],
                    common[2],
                    common[3],
                    common[4],
                    common[5],
                    common[6],
                    common[7],
                    common[8],
                    common[9],
                    common[10],
                    common[11],
                    checked::ObjectStorage(reciprocal_condition)};
}

template <typename T, typename FactorElement, typename Pivot>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const T> original,
    LapackTridiagonalLuStorage<FactorElement> factors, Pivot pivots,
    DenseBlasMatrixView<const T> rhs, DenseBlasMatrixView<T> solution,
    const DenseBlasRealType<T>& reciprocal_condition,
    DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
    DenseBlasVectorView<DenseBlasRealType<T>> backward_error) {
  const Status status = checked::Disjoint(
      Operands(original, factors, pivots, rhs, solution, reciprocal_condition,
               forward_error, backward_error));
  if (!status.ok()) {
    return status;
  }
  return checked::QueryExpert(provider, Native<T>::kName, transpose,
                              std::is_const_v<FactorElement> ? 1 : 0, true,
                              original, factors, pivots, rhs, solution,
                              forward_error, backward_error);
}

template <bool Factored, typename T, typename Pivot>
Status Finish(extent_t order, lapack_int info, const lapack_int* native_pivots,
              Pivot pivots, DenseBlasMatrixView<T> solution,
              const T* packed_solution,
              DenseBlasRealType<T>& reciprocal_condition,
              DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
              DenseBlasVectorView<DenseBlasRealType<T>> backward_error,
              LapackReport& report) {
  report.native_info = info;
  if (info < 0 || info > order + 1 ||
      (Factored && info != 0 && info != order + 1) ||
      !checked::NativePivotValues(native_pivots, order).ok()) {
    return checked::ProviderDefect(info, report);
  }
  if constexpr (!Factored) {
    for (extent_t i = 0; i < order; ++i) {
      pivots.data()[i] = native_pivots[i];
    }
  }
  if (info > 0 && info <= order) {
    if (reciprocal_condition != DenseBlasRealType<T>{0}) {
      return checked::ProviderDefect(info, report);
    }
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  internal_lapack_layout::Unpack(packed_solution, solution);
  Status condition_status = checked::Diagnostic(reciprocal_condition, report);
  if (condition_status.code() == ErrorCode::kProvider) {
    return condition_status;
  }
  Status status = checked::Errors(forward_error, backward_error, report);
  if (!status.ok()) {
    return status;
  }
  if (!condition_status.ok()) {
    return condition_status;
  }
  if (info == order + 1) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T, typename FactorElement, typename Pivot>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose transpose,
               LapackTridiagonalView<const T> original,
               LapackTridiagonalLuStorage<FactorElement> factors, Pivot pivots,
               DenseBlasMatrixView<const T> rhs,
               DenseBlasMatrixView<T> solution,
               DenseBlasRealType<T>& reciprocal_condition,
               DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
               DenseBlasVectorView<DenseBlasRealType<T>> backward_error,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  constexpr bool kFactored = std::is_const_v<FactorElement>;
  using Real = DenseBlasRealType<T>;
  const auto operands =
      Operands(original, factors, pivots, rhs, solution, reciprocal_condition,
               forward_error, backward_error);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  const auto expected =
      Query(provider, transpose, original, factors, pivots, rhs, solution,
            reciprocal_condition, forward_error, backward_error);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t order = original.order();
  if (order == 0) {
    reciprocal_condition = 1;
    checked::EmptyErrors(forward_error, backward_error);
    return checked::Complete(report);
  }
  lapack_int* native_pivots = nullptr;
  if constexpr (kFactored) {
    status = checked::PivotValues(pivots);
    if (!status.ok()) {
      return status;
    }
    status = checked::ZeroDiagonal(factors, report);
    if (!status.ok()) {
      return status;
    }
    native_pivots = checked::ConvertPivots(pivots, workspace);
  } else {
    native_pivots = checked::OutputPivotObjects(order, workspace);
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const T* packed_rhs = internal_lapack_layout::Pack(rhs, cursor);
  T* packed_solution = checked::Output(solution, cursor);
  const std::array<T, 3> input_dummies{};
  std::array<T, 4> factor_dummies{};
  std::array<Real, 2> error_dummies{};
  // The exact FACT=F path and GTCON/GTTRS/GTRFS callees read all four factor
  // arrays only. The upstream combined N/F prototype lacks conditional const.
  // Removing the qualifier here does not authorize or perform an F-mode write.
  T* factor_lower = const_cast<T*>(factors.primary().lower().data());
  T* factor_diagonal = const_cast<T*>(factors.primary().diagonal().data());
  T* factor_upper = const_cast<T*>(factors.primary().upper().data());
  T* second = const_cast<T*>(factors.second_upper().data());
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      kFactored ? 'F' : 'N', checked::TransposeCharacter(transpose),
      static_cast<lapack_int>(order), static_cast<lapack_int>(rhs.columns()),
      checked::Nonnull<const T>(original.lower().data(), input_dummies[0]),
      original.diagonal().data(),
      checked::Nonnull<const T>(original.upper().data(), input_dummies[1]),
      checked::Nonnull(factor_lower, factor_dummies[0]), factor_diagonal,
      checked::Nonnull(factor_upper, factor_dummies[1]),
      checked::Nonnull(second, factor_dummies[2]), native_pivots,
      checked::Nonnull<const T>(packed_rhs, input_dummies[2]),
      static_cast<lapack_int>(checked::Leading(rhs)),
      checked::Nonnull(packed_solution, factor_dummies[3]),
      static_cast<lapack_int>(checked::Leading(solution)), reciprocal_condition,
      checked::Nonnull(forward_error.data(), error_dummies[0]),
      checked::Nonnull(backward_error.data(), error_dummies[1]), workspace);
  return Finish<kFactored>(order, info, native_pivots, pivots, solution,
                           packed_solution, reciprocal_condition, forward_error,
                           backward_error, report);
}

}  // namespace

Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<float> factors,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, const float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status Gtsvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const float> original,
             LapackTridiagonalLuStorage<float> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution, float& reciprocal_condition,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, const float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<double> factors,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status Gtsvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const double> original,
             LapackTridiagonalLuStorage<double> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution, double& reciprocal_condition,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status Gtsvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const std::complex<float>> original,
             LapackTridiagonalLuStorage<std::complex<float>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             float& reciprocal_condition,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status Gtsvx(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const std::complex<double>> original,
             LapackTridiagonalLuStorage<std::complex<double>> factors,
             DenseBlasVectorView<index_t> pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             double& reciprocal_condition,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return Query(provider, transpose, original, factors, pivots, rhs, solution,
               reciprocal_condition, forward_error, backward_error);
}
Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 reciprocal_condition, forward_error, backward_error, plan,
                 workspace, report);
}

}  // namespace asc
