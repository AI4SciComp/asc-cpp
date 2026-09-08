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
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_condition.h"
#include "internal_cholesky_expert.h"
#include "internal_indefinite.h"
#include "internal_packed_cholesky_condition_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace checked = internal_cholesky_expert;
namespace common = internal_indefinite;
namespace storage = internal_packed_triangular;

template <typename T>
struct Native;

template <>
struct Native<float> {
  static constexpr auto kKind = LapackScalarKind::kF32;
  static constexpr std::string_view kName = "sppcon";
  static lapack_int Execute(char triangle, lapack_int n, const float* a,
                            float norm, float* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work =
        static_cast<float*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<float>(n, workspace);
    LAPACK_sppcon(&triangle, &n, a, &norm, condition, work, extra, &info);
    return info;
  }
};

template <>
struct Native<double> {
  static constexpr auto kKind = LapackScalarKind::kF64;
  static constexpr std::string_view kName = "dppcon";
  static lapack_int Execute(char triangle, lapack_int n, const double* a,
                            double norm, double* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work =
        static_cast<double*>(workspace.regions[checked::kScalar].data());
    auto* extra = checked::IntegerWork<double>(n, workspace);
    LAPACK_dppcon(&triangle, &n, a, &norm, condition, work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<float>> {
  static constexpr auto kKind = LapackScalarKind::kC64;
  static constexpr std::string_view kName = "cppcon";
  static lapack_int Execute(char triangle, lapack_int n,
                            const std::complex<float>* a, float norm,
                            float* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work = static_cast<std::complex<float>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra = static_cast<float*>(workspace.regions[checked::kReal].data());
    LAPACK_cppcon(&triangle, &n, a, &norm, condition, work, extra, &info);
    return info;
  }
};

template <>
struct Native<std::complex<double>> {
  static constexpr auto kKind = LapackScalarKind::kC128;
  static constexpr std::string_view kName = "zppcon";
  static lapack_int Execute(char triangle, lapack_int n,
                            const std::complex<double>* a, double norm,
                            double* condition,
                            const LapackWorkspace& workspace) {
    lapack_int info = std::numeric_limits<lapack_int>::min();
    auto* work = static_cast<std::complex<double>*>(
        workspace.regions[checked::kScalar].data());
    auto* extra =
        static_cast<double*>(workspace.regions[checked::kReal].data());
    LAPACK_zppcon(&triangle, &n, a, &norm, condition, work, extra, &info);
    return info;
  }
};

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const T> factors,
    DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition) {
  if (!common::Triangle(triangle) || !std::isfinite(original_norm) ||
      original_norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array operands{factors.reachable_storage(),
                            common::Object(reciprocal_condition)};
  for (const auto operand : operands) {
    Status status = common::Accessible(provider, operand);
    if (!status.ok()) {
      return status;
    }
  }
  Status status = common::Disjoint(operands);
  if (!status.ok()) {
    return status;
  }
  const bool active = factors.order() != 0 && original_norm != 0;
  const extent_t order = active ? factors.order() : 0;
  status = internal_packed_cholesky_condition_counts::Condition(
      order, common::kIntegerLimit);
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Native<T>::kName, Native<T>::kKind, std::array{factors.order()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(factors.layout()),
                                  static_cast<std::int64_t>(active)},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  status = checked::AddEstimatorWork<T>(order, plan);
  if (!status.ok()) {
    return status;
  }
  if (active) {
    status = storage::Packing(factors, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename Real>
Status ValidateEstimate(Real value, LapackReport& report) {
  if (value < 0) {
    return common::Defect(0, report);
  }
  if (!std::isfinite(value)) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    return Status(ErrorCode::kNumerical);
  }
  return common::Complete(report);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasPackedMatrixView<const T> factors,
               DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array operands{factors.reachable_storage(),
                            common::Object(reciprocal_condition)};
  Status status = common::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  common::Start(provider, Native<T>::kName, report);
  const auto expected =
      Query(provider, triangle, factors, original_norm, reciprocal_condition);
  if (!expected.ok()) {
    return expected.status();
  }
  status = common::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.order() == 0 || original_norm == 0) {
    reciprocal_condition = factors.order() == 0 ? 1 : 0;
    return common::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const auto* packed =
      storage::Pack(factors, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  reciprocal_condition = std::numeric_limits<DenseBlasRealType<T>>::quiet_NaN();
  report.called_provider = true;
  const auto info = Native<T>::Execute(
      common::Uplo(triangle), static_cast<lapack_int>(factors.order()), packed,
      original_norm, &reciprocal_condition, workspace);
  report.native_info = info;
  if (info != 0) {
    return common::Defect(info, report);
  }
  return ValidateEstimate(reciprocal_condition, report);
}
}  // namespace

Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> factors, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Ppcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const float> factors,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Ppcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const double> factors,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Ppcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<float>> factors,
             float original_norm, float& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, triangle, factors, original_norm,
               reciprocal_condition);
}
Status Ppcon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<const std::complex<double>> factors,
             double original_norm, double& reciprocal_condition,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, factors, original_norm,
                 reciprocal_condition, plan, workspace, report);
}

}  // namespace asc
