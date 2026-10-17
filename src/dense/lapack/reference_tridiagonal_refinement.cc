#include <complex>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_refinement.h"
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
  static constexpr std::string_view kName = "sgtrfs";
  static lapack_int Execute(char transpose, lapack_int n, lapack_int nrhs,
                            const float* lower, const float* diagonal,
                            const float* upper, const float* factor_lower,
                            const float* factor_diagonal,
                            const float* factor_upper, const float* second,
                            const lapack_int* pivots, const float* rhs,
                            lapack_int ldb, float* solution, lapack_int ldx,
                            float* ferr, float* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgtrfs(
        &transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
        factor_diagonal, factor_upper, second, pivots, rhs, &ldb, solution,
        &ldx, ferr, berr,
        static_cast<float*>(workspace.regions[checked::kScalar].data()),
        checked::EstimatorIntegerObjects(n, workspace), &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kName = "dgtrfs";
  static lapack_int Execute(char transpose, lapack_int n, lapack_int nrhs,
                            const double* lower, const double* diagonal,
                            const double* upper, const double* factor_lower,
                            const double* factor_diagonal,
                            const double* factor_upper, const double* second,
                            const lapack_int* pivots, const double* rhs,
                            lapack_int ldb, double* solution, lapack_int ldx,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgtrfs(
        &transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
        factor_diagonal, factor_upper, second, pivots, rhs, &ldb, solution,
        &ldx, ferr, berr,
        static_cast<double*>(workspace.regions[checked::kScalar].data()),
        checked::EstimatorIntegerObjects(n, workspace), &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kName = "cgtrfs";
  static lapack_int Execute(
      char transpose, lapack_int n, lapack_int nrhs,
      const std::complex<float>* lower, const std::complex<float>* diagonal,
      const std::complex<float>* upper, const std::complex<float>* factor_lower,
      const std::complex<float>* factor_diagonal,
      const std::complex<float>* factor_upper,
      const std::complex<float>* second, const lapack_int* pivots,
      const std::complex<float>* rhs, lapack_int ldb,
      std::complex<float>* solution, lapack_int ldx, float* ferr, float* berr,
      const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgtrfs(&transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
                  factor_diagonal, factor_upper, second, pivots, rhs, &ldb,
                  solution, &ldx, ferr, berr,
                  static_cast<std::complex<float>*>(
                      workspace.regions[checked::kScalar].data()),
                  static_cast<float*>(workspace.regions[checked::kReal].data()),
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kName = "zgtrfs";
  static lapack_int Execute(char transpose, lapack_int n, lapack_int nrhs,
                            const std::complex<double>* lower,
                            const std::complex<double>* diagonal,
                            const std::complex<double>* upper,
                            const std::complex<double>* factor_lower,
                            const std::complex<double>* factor_diagonal,
                            const std::complex<double>* factor_upper,
                            const std::complex<double>* second,
                            const lapack_int* pivots,
                            const std::complex<double>* rhs, lapack_int ldb,
                            std::complex<double>* solution, lapack_int ldx,
                            double* ferr, double* berr,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgtrfs(
        &transpose, &n, &nrhs, lower, diagonal, upper, factor_lower,
        factor_diagonal, factor_upper, second, pivots, rhs, &ldb, solution,
        &ldx, ferr, berr,
        static_cast<std::complex<double>*>(
            workspace.regions[checked::kScalar].data()),
        static_cast<double*>(workspace.regions[checked::kReal].data()), &info);
    return info;
  }
};

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTranspose transpose,
               LapackTridiagonalView<const T> original,
               LapackTridiagonalLuStorage<const T> factors,
               ReferenceTridiagonalPivotView pivots,
               DenseBlasMatrixView<const T> rhs,
               DenseBlasMatrixView<T> solution,
               DenseBlasVectorView<DenseBlasRealType<T>> forward_error,
               DenseBlasVectorView<DenseBlasRealType<T>> backward_error,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = checked::ExpertOperands(
      original, factors, pivots, rhs, solution, forward_error, backward_error);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  const auto expected = checked::QueryExpert(
      provider, Native<T>::kName, transpose, 0, false, original, factors,
      pivots, rhs, solution, forward_error, backward_error);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  const extent_t order = original.order();
  if (order == 0 || rhs.columns() == 0) {
    checked::EmptyErrors(forward_error, backward_error);
    return checked::Complete(report);
  }
  status = checked::PivotValues(pivots);
  if (!status.ok()) {
    return status;
  }
  status = checked::ZeroDiagonal(factors, report);
  if (!status.ok()) {
    return status;
  }
  const auto* native_pivots = checked::ConvertPivots(pivots, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const T* packed_rhs = internal_lapack_layout::Pack(rhs, cursor);
  T* packed_solution = internal_lapack_layout::Pack(solution, cursor);
  const T lower_dummy{};
  const T upper_dummy{};
  const T factor_lower_dummy{};
  const T factor_upper_dummy{};
  const T second_dummy{};
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      checked::TransposeCharacter(transpose), static_cast<lapack_int>(order),
      static_cast<lapack_int>(rhs.columns()),
      checked::Nonnull<const T>(original.lower().data(), lower_dummy),
      original.diagonal().data(),
      checked::Nonnull<const T>(original.upper().data(), upper_dummy),
      checked::Nonnull<const T>(factors.primary().lower().data(),
                                factor_lower_dummy),
      factors.primary().diagonal().data(),
      checked::Nonnull<const T>(factors.primary().upper().data(),
                                factor_upper_dummy),
      checked::Nonnull<const T>(factors.second_upper().data(), second_dummy),
      native_pivots, packed_rhs, static_cast<lapack_int>(checked::Leading(rhs)),
      packed_solution, static_cast<lapack_int>(checked::Leading(solution)),
      forward_error.data(), backward_error.data(), workspace);
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  internal_lapack_layout::Unpack(packed_solution, solution);
  status = checked::Errors(forward_error, backward_error, report);
  return status.ok() ? checked::Complete(report) : status;
}

}  // namespace

Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return checked::QueryExpert(provider, Native<float>::kName, transpose, 0,
                              false, original, factors, pivots, rhs, solution,
                              forward_error, backward_error);
}
Status Gtrfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const float> original,
             LapackTridiagonalLuStorage<const float> factors,
             ReferenceTridiagonalPivotView pivots,
             DenseBlasMatrixView<const float> rhs,
             DenseBlasMatrixView<float> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 forward_error, backward_error, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return checked::QueryExpert(provider, Native<double>::kName, transpose, 0,
                              false, original, factors, pivots, rhs, solution,
                              forward_error, backward_error);
}
Status Gtrfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const double> original,
             LapackTridiagonalLuStorage<const double> factors,
             ReferenceTridiagonalPivotView pivots,
             DenseBlasMatrixView<const double> rhs,
             DenseBlasMatrixView<double> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 forward_error, backward_error, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error) {
  return checked::QueryExpert(provider, Native<std::complex<float>>::kName,
                              transpose, 0, false, original, factors, pivots,
                              rhs, solution, forward_error, backward_error);
}
Status Gtrfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const std::complex<float>> original,
             LapackTridiagonalLuStorage<const std::complex<float>> factors,
             ReferenceTridiagonalPivotView pivots,
             DenseBlasMatrixView<const std::complex<float>> rhs,
             DenseBlasMatrixView<std::complex<float>> solution,
             DenseBlasVectorView<float> forward_error,
             DenseBlasVectorView<float> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 forward_error, backward_error, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error) {
  return checked::QueryExpert(provider, Native<std::complex<double>>::kName,
                              transpose, 0, false, original, factors, pivots,
                              rhs, solution, forward_error, backward_error);
}
Status Gtrfs(const ReferenceLapackProvider& provider,
             DenseBlasTranspose transpose,
             LapackTridiagonalView<const std::complex<double>> original,
             LapackTridiagonalLuStorage<const std::complex<double>> factors,
             ReferenceTridiagonalPivotView pivots,
             DenseBlasMatrixView<const std::complex<double>> rhs,
             DenseBlasMatrixView<std::complex<double>> solution,
             DenseBlasVectorView<double> forward_error,
             DenseBlasVectorView<double> backward_error,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, transpose, original, factors, pivots, rhs, solution,
                 forward_error, backward_error, plan, workspace, report);
}

}  // namespace asc
