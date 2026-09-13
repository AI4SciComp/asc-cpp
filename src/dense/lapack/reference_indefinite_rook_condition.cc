#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook_condition.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"
#include "internal_indefinite_rook_condition_prototypes.h"
#include "internal_indefinite_rook_pivots.h"

namespace asc {
namespace {

namespace bk = internal_indefinite;
namespace expert = internal_indefinite_expert;

template <typename T>
std::string_view Name(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssycon_rook";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsycon_rook";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "checon_rook" : "csycon_rook";
  } else {
    return hermitian ? "zhecon_rook" : "zsycon_rook";
  }
}

// Preserve the compiler-emitted non-const pointer signature at this private
// boundary. The pinned source consumes A and IPIV without modifying them.
template <typename T>
void Call(bool hermitian, char triangle, lapack_int n, const T* a,
          lapack_int lda, lapack_int* pivots, DenseBlasRealType<T> norm,
          DenseBlasRealType<T>* condition, T* work, lapack_int& info) {
  T* input = const_cast<T*>(a);
  if constexpr (std::is_same_v<T, float>) {
    ssycon_rook_(&triangle, &n, input, &lda, pivots, &norm, condition, work,
                 pivots + n, &info, std::size_t{1});
  } else if constexpr (std::is_same_v<T, double>) {
    dsycon_rook_(&triangle, &n, input, &lda, pivots, &norm, condition, work,
                 pivots + n, &info, std::size_t{1});
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    const auto call = hermitian ? checon_rook_ : csycon_rook_;
    call(&triangle, &n, input, &lda, pivots, &norm, condition, work, &info,
         std::size_t{1});
  } else {
    const auto call = hermitian ? zhecon_rook_ : zsycon_rook_;
    call(&triangle, &n, input, &lda, pivots, &norm, condition, work, &info,
         std::size_t{1});
  }
}

Status PivotMetadata(const ReferenceLapackProvider& provider,
                     RawLapackPivotView pivots, extent_t order) {
  if (pivots.family() != LapackFactorFamily::kRook) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (pivots.values().size() != static_cast<std::size_t>(order)) {
    return Status(ErrorCode::kShape);
  }
  return bk::Accessible(provider, pivots.reachable_storage());
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
        PivotMetadata(provider, pivots, factors.rows()), bk::Disjoint(spans)}) {
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
  status = internal_indefinite_rook::Paired(pivots.values(), factors.rows(),
                                            triangle);
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
  lapack_int info = std::numeric_limits<lapack_int>::min();
  reciprocal_condition = -1;
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

Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status SyconRook(const ReferenceLapackProvider& provider,
                 DenseBlasTriangle triangle,
                 DenseBlasMatrixView<const float> factors,
                 RawLapackPivotView pivots, float original_norm,
                 float& reciprocal_condition, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status SyconRook(const ReferenceLapackProvider& provider,
                 DenseBlasTriangle triangle,
                 DenseBlasMatrixView<const double> factors,
                 RawLapackPivotView pivots, double original_norm,
                 double& reciprocal_condition, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status SyconRook(const ReferenceLapackProvider& provider,
                 DenseBlasTriangle triangle,
                 DenseBlasMatrixView<const std::complex<float>> factors,
                 RawLapackPivotView pivots, float original_norm,
                 float& reciprocal_condition, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, false);
}

Status SyconRook(const ReferenceLapackProvider& provider,
                 DenseBlasTriangle triangle,
                 DenseBlasMatrixView<const std::complex<double>> factors,
                 RawLapackPivotView pivots, double original_norm,
                 double& reciprocal_condition, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QueryHeconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, true);
}

Status HeconRook(const ReferenceLapackProvider& provider,
                 DenseBlasTriangle triangle,
                 DenseBlasMatrixView<const std::complex<float>> factors,
                 RawLapackPivotView pivots, float original_norm,
                 float& reciprocal_condition, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, true);
}

Result<LapackWorkspacePlan> QueryHeconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, pivots, original_norm,
               reciprocal_condition, true);
}

Status HeconRook(const ReferenceLapackProvider& provider,
                 DenseBlasTriangle triangle,
                 DenseBlasMatrixView<const std::complex<double>> factors,
                 RawLapackPivotView pivots, double original_norm,
                 double& reciprocal_condition, const LapackWorkspacePlan& plan,
                 const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, original_norm,
                 reciprocal_condition, plan, workspace, report, true);
}

}  // namespace asc
