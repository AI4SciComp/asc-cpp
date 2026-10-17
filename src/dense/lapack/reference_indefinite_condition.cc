#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_condition.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"

namespace asc {
namespace {

namespace bk = internal_indefinite;
namespace expert = internal_indefinite_expert;

template <typename T>
std::string_view Name(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssycon";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsycon";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "checon" : "csycon";
  } else {
    return hermitian ? "zhecon" : "zsycon";
  }
}

void Call([[maybe_unused]] bool hermitian, char triangle, lapack_int n,
          const float* a, lapack_int lda, lapack_int* pivots, float norm,
          float* condition, float* work, lapack_int& info) {
  LAPACK_ssycon(&triangle, &n, a, &lda, pivots, &norm, condition, work,
                pivots + n, &info);
}

void Call([[maybe_unused]] bool hermitian, char triangle, lapack_int n,
          const double* a, lapack_int lda, lapack_int* pivots, double norm,
          double* condition, double* work, lapack_int& info) {
  LAPACK_dsycon(&triangle, &n, a, &lda, pivots, &norm, condition, work,
                pivots + n, &info);
}

void Call(bool hermitian, char triangle, lapack_int n,
          const std::complex<float>* a, lapack_int lda, lapack_int* pivots,
          float norm, float* condition, std::complex<float>* work,
          lapack_int& info) {
  if (hermitian) {
    LAPACK_checon(&triangle, &n, a, &lda, pivots, &norm, condition, work,
                  &info);
  } else {
    LAPACK_csycon(&triangle, &n, a, &lda, pivots, &norm, condition, work,
                  &info);
  }
}

void Call(bool hermitian, char triangle, lapack_int n,
          const std::complex<double>* a, lapack_int lda, lapack_int* pivots,
          double norm, double* condition, std::complex<double>* work,
          lapack_int& info) {
  if (hermitian) {
    LAPACK_zhecon(&triangle, &n, a, &lda, pivots, &norm, condition, work,
                  &info);
  } else {
    LAPACK_zsycon(&triangle, &n, a, &lda, pivots, &norm, condition, work,
                  &info);
  }
}

template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const T> factors, RawLapackPivotView pivots,
    DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition, bool hermitian) {
  if (!bk::Triangle(triangle) || !std::isfinite(original_norm) ||
      original_norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{
      factors.reachable_storage(), pivots.reachable_storage(),
      bk::Object(reciprocal_condition), bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, factors),
        expert::PivotMetadata(provider, pivots, factors.rows()),
        bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const bool active = factors.rows() != 0 && original_norm != 0;
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(hermitian), bk::ScalarKind<T>(),
      std::array{factors.rows(), factors.columns(), bk::Leading(factors)},
      std::array<std::int64_t, 5>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(hermitian),
                                  static_cast<std::int64_t>(factors.layout()),
                                  factors.leading_dimension(),
                                  static_cast<std::int64_t>(active)},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (active) {
    for (const Status& status :
         {expert::EstimatorWork<T>(factors.rows(), false, plan),
          bk::Packing(factors, plan)}) {
      if (!status.ok()) {
        return status;
      }
    }
  }
  return plan;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<const T> factors,
               RawLapackPivotView pivots, DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage(),
                            bk::Object(reciprocal_condition)};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  const auto expected = Query(provider, triangle, factors, pivots,
                              original_norm, reciprocal_condition, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() == 0 || original_norm == 0) {
    reciprocal_condition = factors.rows() == 0 ? 1 : 0;
    return bk::Complete(report);
  }
  status = bk::Paired(pivots.values(), factors.rows(), triangle);
  if (!status.ok()) {
    return status;
  }
  status =
      expert::BlockDivisors(factors, pivots, triangle, hermitian, true, report);
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots = expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* packed = bk::PackTriangle(factors, triangle, false, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  lapack_int info = 0;
  report.called_provider = true;
  Call(hermitian, bk::Uplo(triangle), static_cast<lapack_int>(factors.rows()),
       packed, static_cast<lapack_int>(bk::Leading(factors)), native_pivots,
       original_norm, &reciprocal_condition, work, info);
  report.native_info = info;
  if (info != 0) {
    return bk::Defect(info, report);
  }
  return expert::Estimate(reciprocal_condition, report);
}

}  // namespace

Result<LapackWorkspacePlan> QuerySyconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Sycon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const float> factors,
             RawLapackPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySyconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Sycon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const double> factors,
             RawLapackPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySyconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Sycon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySyconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status Sycon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QueryHeconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, true);
}

Status Hecon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<float>> factors,
             RawLapackPivotView pivots, float original_norm,
             float& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, true);
}

Result<LapackWorkspacePlan> QueryHeconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, true);
}

Status Hecon(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<const std::complex<double>> factors,
             RawLapackPivotView pivots, double original_norm,
             double& reciprocal_condition, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, true);
}

}  // namespace asc
