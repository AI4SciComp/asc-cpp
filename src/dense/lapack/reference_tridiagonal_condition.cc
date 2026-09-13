#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_condition.h"
#include "internal_tridiagonal.h"
#include "internal_tridiagonal_counts.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr std::string_view kName = "sgtcon";
  static lapack_int Execute(char norm, lapack_int n, const float* lower,
                            const float* diagonal, const float* upper,
                            const float* second, const lapack_int* pivots,
                            float anorm, float& rcond,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_sgtcon(
        &norm, &n, lower, diagonal, upper, second, pivots, &anorm, &rcond,
        static_cast<float*>(workspace.regions[checked::kScalar].data()),
        checked::EstimatorIntegerObjects(n, workspace), &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr std::string_view kName = "dgtcon";
  static lapack_int Execute(char norm, lapack_int n, const double* lower,
                            const double* diagonal, const double* upper,
                            const double* second, const lapack_int* pivots,
                            double anorm, double& rcond,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_dgtcon(
        &norm, &n, lower, diagonal, upper, second, pivots, &anorm, &rcond,
        static_cast<double*>(workspace.regions[checked::kScalar].data()),
        checked::EstimatorIntegerObjects(n, workspace), &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr std::string_view kName = "cgtcon";
  static lapack_int Execute(char norm, lapack_int n,
                            const std::complex<float>* lower,
                            const std::complex<float>* diagonal,
                            const std::complex<float>* upper,
                            const std::complex<float>* second,
                            const lapack_int* pivots, float anorm, float& rcond,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_cgtcon(&norm, &n, lower, diagonal, upper, second, pivots, &anorm,
                  &rcond,
                  static_cast<std::complex<float>*>(
                      workspace.regions[checked::kScalar].data()),
                  &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr std::string_view kName = "zgtcon";
  static lapack_int Execute(char norm, lapack_int n,
                            const std::complex<double>* lower,
                            const std::complex<double>* diagonal,
                            const std::complex<double>* upper,
                            const std::complex<double>* second,
                            const lapack_int* pivots, double anorm,
                            double& rcond, const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    LAPACK_zgtcon(&norm, &n, lower, diagonal, upper, second, pivots, &anorm,
                  &rcond,
                  static_cast<std::complex<double>*>(
                      workspace.regions[checked::kScalar].data()),
                  &info);
    return info;
  }
};

template <typename T>
auto Operands(LapackTridiagonalLuStorage<const T> factors,
              ReferenceTridiagonalPivotView pivots,
              const DenseBlasRealType<T>& reciprocal_condition) {
  const auto spans = checked::Spans(factors);
  return std::array{spans[0],
                    spans[1],
                    spans[2],
                    spans[3],
                    pivots.reachable_storage(),
                    checked::ObjectStorage(reciprocal_condition)};
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const T> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition) {
  if ((norm != LapackConditionNorm::kOne &&
       norm != LapackConditionNorm::kInfinity) ||
      !std::isfinite(original_norm) || original_norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const extent_t order = factors.primary().order();
  Status status = checked::Storage(provider, factors);
  if (!status.ok()) {
    return status;
  }
  status = checked::PivotMetadata(provider, pivots, order);
  if (!status.ok()) {
    return status;
  }
  status = checked::Disjoint(Operands(factors, pivots, reciprocal_condition));
  if (!status.ok()) {
    return status;
  }
  status = internal_tridiagonal_counts::Estimator(order, checked::kLimit);
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Native<T>::kName, checked::Kind<T>(), std::array{order},
      std::array{static_cast<std::int64_t>(norm)}, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  status = checked::AddEstimator<T>(order, false, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackConditionNorm norm,
               LapackTridiagonalLuStorage<const T> factors,
               ReferenceTridiagonalPivotView pivots,
               DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const auto operands = Operands(factors, pivots, reciprocal_condition);
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  const auto expected = Query(provider, norm, factors, pivots, original_norm,
                              reciprocal_condition);
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
    reciprocal_condition = 1;
    return checked::Complete(report);
  }
  status = checked::PivotValues(pivots);
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots = checked::ConvertPivots(pivots, workspace);
  const T lower_dummy{};
  const T upper_dummy{};
  const T second_dummy{};
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      norm == LapackConditionNorm::kOne ? '1' : 'I',
      static_cast<lapack_int>(order),
      checked::Nonnull<const T>(factors.primary().lower().data(), lower_dummy),
      factors.primary().diagonal().data(),
      checked::Nonnull<const T>(factors.primary().upper().data(), upper_dummy),
      checked::Nonnull<const T>(factors.second_upper().data(), second_dummy),
      native_pivots, original_norm, reciprocal_condition, workspace);
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  status = checked::Diagnostic(reciprocal_condition, report);
  return status.ok() ? checked::Complete(report) : status;
}

}  // namespace

Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackTridiagonalLuStorage<const float> factors,
             ReferenceTridiagonalPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackTridiagonalLuStorage<const double> factors,
             ReferenceTridiagonalPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackTridiagonalLuStorage<const std::complex<float>> factors,
             ReferenceTridiagonalPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, norm, factors, pivots, original_norm,
               reciprocal_condition);
}
Status Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
             LapackTridiagonalLuStorage<const std::complex<double>> factors,
             ReferenceTridiagonalPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, norm, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

}  // namespace asc
