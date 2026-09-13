#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_condition.h"
#include "internal_cholesky_expert.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "spocon";
  static lapack_int Execute(char triangle, lapack_int n, const float* a,
                            lapack_int lda, float norm, float* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work =
        static_cast<float*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<float>(n, workspace);
    LAPACK_spocon(&triangle, &n, a, &lda, &norm, condition, work, extra, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dpocon";
  static lapack_int Execute(char triangle, lapack_int n, const double* a,
                            lapack_int lda, double norm, double* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work =
        static_cast<double*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<double>(n, workspace);
    LAPACK_dpocon(&triangle, &n, a, &lda, &norm, condition, work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cpocon";
  static lapack_int Execute(char triangle, lapack_int n,
                            const std::complex<float>* a, lapack_int lda,
                            float norm, float* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[checked::kReal].data());
    LAPACK_cpocon(&triangle, &n, a, &lda, &norm, condition, work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zpocon";
  static lapack_int Execute(char triangle, lapack_int n,
                            const std::complex<double>* a, lapack_int lda,
                            double norm, double* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = 0;
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra =
        static_cast<double*>(workspace.regions[checked::kReal].data());
    LAPACK_zpocon(&triangle, &n, a, &lda, &norm, condition, work, extra, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const T> factors, DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition) {
  Status status = checked::Triangle(triangle);
  if (!status.ok()) {
    return status;
  }
  status = checked::Matrix(provider, factors);
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() != factors.columns()) {
    return Status(ErrorCode::kShape);
  }
  if (!std::isfinite(original_norm) || original_norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (checked::Overlap(factors.reachable_storage(),
                       checked::ObjectStorage(reciprocal_condition))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const bool active = factors.rows() != 0 && original_norm != 0;
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kKind,
      std::array{factors.rows(), factors.columns(), checked::Leading(factors)},
      std::array<std::int64_t, 4>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(factors.layout()),
                                  factors.leading_dimension(),
                                  static_cast<std::int64_t>(active)},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = checked::AddEstimatorWork<T>(active ? factors.rows() : 0, plan);
  if (!status.ok()) {
    return status;
  }
  if (active) {
    status = checked::AddPacking(factors, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename Real>
Status ValidateEstimate(Real value, LapackReport& report) {
  if (value < 0) {
    return checked::ProviderDefect(0, report);
  }
  if (!std::isfinite(value)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return checked::Complete(report);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<const T> factors,
               DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{factors.reachable_storage(),
                            checked::ObjectStorage(reciprocal_condition)};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Native<T>::kName, report);
  const auto expected =
      Query(provider, triangle, factors, original_norm, reciprocal_condition);
  if (!expected.ok()) {
    return expected.status();
  }
  status =
      checked::ValidatePlan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() == 0 || original_norm == 0) {
    reciprocal_condition = factors.rows() == 0 ? 1 : 0;
    return checked::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const auto* packed = checked::PackTriangle(factors, triangle, false, cursor);
  report.called_provider = true;
  const auto info =
      Native<T>::Execute(checked::TriangleCharacter(triangle),
                         static_cast<lapack_int>(factors.rows()), packed,
                         static_cast<lapack_int>(checked::Leading(factors)),
                         original_norm, &reciprocal_condition, workspace);
  report.native_info = info;
  if (info != 0) {
    return checked::ProviderDefect(info, report);
  }
  return ValidateEstimate(reciprocal_condition, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Pocon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const float> factors, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Pocon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const double> factors, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Pocon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> factors,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Pocon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> factors,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

}  // namespace asc
