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
#include "asc/dense/providers/lapack_indefinite_rk_condition.h"
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
    return "ssycon_3";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsycon_3";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "checon_3" : "csycon_3";
  } else {
    return hermitian ? "zhecon_3" : "zsycon_3";
  }
}

template <typename T>
void Call(bool hermitian, char triangle, lapack_int n, const T* a,
          lapack_int lda, const T* off_diagonal, lapack_int* pivots,
          DenseBlasRealType<T> norm, DenseBlasRealType<T>* condition, T* work,
          lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssycon_3(&triangle, &n, a, &lda, off_diagonal, pivots, &norm,
                    condition, work, pivots + n, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsycon_3(&triangle, &n, a, &lda, off_diagonal, pivots, &norm,
                    condition, work, pivots + n, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_checon_3(&triangle, &n, a, &lda, off_diagonal, pivots, &norm,
                      condition, work, &info);
    } else {
      LAPACK_csycon_3(&triangle, &n, a, &lda, off_diagonal, pivots, &norm,
                      condition, work, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhecon_3(&triangle, &n, a, &lda, off_diagonal, pivots, &norm,
                      condition, work, &info);
    } else {
      LAPACK_zsycon_3(&triangle, &n, a, &lda, off_diagonal, pivots, &norm,
                      condition, work, &info);
    }
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
    DenseBlasMatrixView<const T> factors,
    DenseBlasVectorView<const T> off_diagonal, RawLapackPivotView pivots,
    DenseBlasRealType<T> original_norm,
    const DenseBlasRealType<T>& reciprocal_condition, bool hermitian) {
  if (!bk::Triangle(triangle) || !std::isfinite(original_norm) ||
      original_norm < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns() ||
      off_diagonal.size() != factors.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (off_diagonal.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{
      factors.reachable_storage(), off_diagonal.reachable_storage(),
      pivots.reachable_storage(), bk::Object(reciprocal_condition),
      bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, factors),
        bk::Accessible(provider, off_diagonal.reachable_storage()),
        PivotMetadata(provider, pivots, factors.rows()), bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const bool active = factors.rows() != 0 && original_norm != 0;
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(hermitian), bk::ScalarKind<T>(),
      std::array{factors.rows(), factors.columns(), bk::Leading(factors),
                 off_diagonal.size(),
                 static_cast<extent_t>(pivots.values().size())},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(hermitian),
          static_cast<std::int64_t>(factors.layout()),
          factors.leading_dimension(), static_cast<std::int64_t>(active),
          off_diagonal.increment(), static_cast<std::int64_t>(pivots.family())},
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

// RK uses each pivot's own directional target; old ROOK pair-wide bounds
// describe a different factor representation. Values alone do not prove origin.
Status PivotValues(RawLapackPivotView pivots, extent_t order,
                   DenseBlasTriangle triangle) {
  const bool upper = triangle == DenseBlasTriangle::kUpper;
  const auto values = pivots.values();
  for (extent_t i = 0; i < order;) {
    const index_t p = values[static_cast<std::size_t>(i)];
    if (p == 0 || p < -order || p > order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const index_t target = p < 0 ? -p : p;
    if ((upper && target > i + 1) || (!upper && target < i + 1)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (p > 0) {
      ++i;
      continue;
    }
    if (i + 1 >= order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const index_t q = values[static_cast<std::size_t>(i + 1)];
    if (q >= 0 || q < -order || (upper && -q > i + 2) ||
        (!upper && -q < i + 2)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    i += 2;
  }
  return Status::Ok();
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<const T> factors,
               DenseBlasVectorView<const T> off_diagonal,
               RawLapackPivotView pivots, DenseBlasRealType<T> original_norm,
               DenseBlasRealType<T>& reciprocal_condition,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{
      factors.reachable_storage(), off_diagonal.reachable_storage(),
      pivots.reachable_storage(), bk::Object(reciprocal_condition)};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  report.factor_family = LapackFactorFamily::kRook;
  const auto expected = Query(provider, triangle, factors, off_diagonal, pivots,
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
  status = PivotValues(pivots, factors.rows(), triangle);
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
       packed, static_cast<lapack_int>(bk::Leading(factors)),
       off_diagonal.data(), native_pivots, original_norm, &reciprocal_condition,
       work, info);
  report.native_info = info;
  if (info != 0) {
    return bk::Defect(info, report);
  }
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  return expert::Estimate(reciprocal_condition, report);
}

}  // namespace
Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition) {
  return Query(provider, triangle, factors, off_diagonal, pivots, original_norm,
               reciprocal_condition, false);
}
Status Sycon3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const float> factors,
              DenseBlasVectorView<const float> off_diagonal,
              RawLapackPivotView pivots, float original_norm,
              float& reciprocal_condition, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots,
                 original_norm, reciprocal_condition, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition) {
  return Query(provider, triangle, factors, off_diagonal, pivots, original_norm,
               reciprocal_condition, false);
}
Status Sycon3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const double> factors,
              DenseBlasVectorView<const double> off_diagonal,
              RawLapackPivotView pivots, double original_norm,
              double& reciprocal_condition, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots,
                 original_norm, reciprocal_condition, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, off_diagonal, pivots, original_norm,
               reciprocal_condition, false);
}
Status Sycon3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<float>> factors,
              DenseBlasVectorView<const std::complex<float>> off_diagonal,
              RawLapackPivotView pivots, float original_norm,
              float& reciprocal_condition, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots,
                 original_norm, reciprocal_condition, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, off_diagonal, pivots, original_norm,
               reciprocal_condition, false);
}
Status Sycon3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<double>> factors,
              DenseBlasVectorView<const std::complex<double>> off_diagonal,
              RawLapackPivotView pivots, double original_norm,
              double& reciprocal_condition, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots,
                 original_norm, reciprocal_condition, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QueryHecon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition) {
  return Query(provider, triangle, factors, off_diagonal, pivots, original_norm,
               reciprocal_condition, true);
}
Status Hecon3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<float>> factors,
              DenseBlasVectorView<const std::complex<float>> off_diagonal,
              RawLapackPivotView pivots, float original_norm,
              float& reciprocal_condition, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots,
                 original_norm, reciprocal_condition, plan, workspace, report,
                 true);
}
Result<LapackWorkspacePlan> QueryHecon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition) {
  return Query(provider, triangle, factors, off_diagonal, pivots, original_norm,
               reciprocal_condition, true);
}
Status Hecon3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<double>> factors,
              DenseBlasVectorView<const std::complex<double>> off_diagonal,
              RawLapackPivotView pivots, double original_norm,
              double& reciprocal_condition, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots,
                 original_norm, reciprocal_condition, plan, workspace, report,
                 true);
}
}  // namespace asc
