#include <array>
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
#include "asc/dense/providers/lapack_indefinite_rk_solve.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_counts.h"
#include "internal_indefinite_expert.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
template <typename T>
std::string_view Name(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssytrs_3";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsytrs_3";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "chetrs_3" : "csytrs_3";
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    return hermitian ? "zhetrs_3" : "zsytrs_3";
  }
}
template <typename T>
void Call(bool hermitian, char triangle, lapack_int n, lapack_int nrhs,
          const T* a, lapack_int lda, const T* extra, const lapack_int* pivots,
          T* b, lapack_int ldb, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_3(&triangle, &n, &nrhs, a, &lda, extra, pivots, b, &ldb,
                    &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_3(&triangle, &n, &nrhs, a, &lda, extra, pivots, b, &ldb,
                    &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrs_3(&triangle, &n, &nrhs, a, &lda, extra, pivots, b, &ldb,
                      &info);
    } else {
      LAPACK_csytrs_3(&triangle, &n, &nrhs, a, &lda, extra, pivots, b, &ldb,
                      &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      LAPACK_zhetrs_3(&triangle, &n, &nrhs, a, &lda, extra, pivots, b, &ldb,
                      &info);
    } else {
      LAPACK_zsytrs_3(&triangle, &n, &nrhs, a, &lda, extra, pivots, b, &ldb,
                      &info);
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
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<const T> factors,
                                  DenseBlasVectorView<const T> off_diagonal,
                                  RawLapackPivotView pivots,
                                  DenseBlasMatrixView<T> rhs, bool hermitian) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns() || factors.rows() != rhs.rows() ||
      off_diagonal.size() != factors.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (off_diagonal.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.leading_dimension() > bk::kIntegerLimit ||
      rhs.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{factors.reachable_storage(),
                         off_diagonal.reachable_storage(),
                         pivots.reachable_storage(), rhs.reachable_storage(),
                         bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, factors), bk::Matrix(provider, rhs),
        bk::Accessible(provider, off_diagonal.reachable_storage()),
        PivotMetadata(provider, pivots, factors.rows()), bk::Disjoint(spans),
        internal_indefinite_counts::Solve(factors.rows(), rhs.columns(),
                                          bk::Leading(rhs),
                                          bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(hermitian), bk::ScalarKind<T>(),
      std::array{factors.rows(), rhs.columns(), bk::Leading(factors),
                 bk::Leading(rhs), off_diagonal.size(),
                 static_cast<extent_t>(pivots.values().size())},
      std::array<std::int64_t, 8>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(hermitian),
          static_cast<std::int64_t>(factors.layout()),
          factors.leading_dimension(), static_cast<std::int64_t>(rhs.layout()),
          rhs.leading_dimension(), off_diagonal.increment(),
          static_cast<std::int64_t>(pivots.family())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.rows() == 0 || rhs.columns() == 0) {
    return plan;
  }
  const auto n = factors.rows();
  plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  for (const Status& status :
       {bk::Packing(factors, plan), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
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
               RawLapackPivotView pivots, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{
      factors.reachable_storage(), off_diagonal.reachable_storage(),
      pivots.reachable_storage(), rhs.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  report.factor_family = LapackFactorFamily::kRook;
  const auto expected =
      Query(provider, triangle, factors, off_diagonal, pivots, rhs, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.rows() == 0 || rhs.columns() == 0) {
    return bk::Complete(report);
  }
  status = PivotValues(pivots, factors.rows(), triangle);
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots =
      internal_indefinite_expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* packed = bk::PackTriangle(factors, triangle, false, cursor);
  T* packed_rhs = bk::PackRhs(rhs, cursor);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(hermitian, bk::Uplo(triangle), static_cast<lapack_int>(factors.rows()),
       static_cast<lapack_int>(rhs.columns()), packed,
       static_cast<lapack_int>(bk::Leading(factors)), off_diagonal.data(),
       native_pivots, packed_rhs, static_cast<lapack_int>(bk::Leading(rhs)),
       info);
  report.native_info = info;
  if (info != 0) {
    return bk::Defect(info, report);
  }
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  bk::PublishRhs(packed_rhs, rhs);
  return bk::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, factors, off_diagonal, pivots, rhs, false);
}
Status Sytrs3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const float> factors,
              DenseBlasVectorView<const float> off_diagonal,
              RawLapackPivotView pivots, DenseBlasMatrixView<float> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, rhs, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, factors, off_diagonal, pivots, rhs, false);
}
Status Sytrs3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const double> factors,
              DenseBlasVectorView<const double> off_diagonal,
              RawLapackPivotView pivots, DenseBlasMatrixView<double> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, rhs, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, off_diagonal, pivots, rhs, false);
}
Status Sytrs3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<float>> factors,
              DenseBlasVectorView<const std::complex<float>> off_diagonal,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, rhs, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, off_diagonal, pivots, rhs, false);
}
Status Sytrs3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<double>> factors,
              DenseBlasVectorView<const std::complex<double>> off_diagonal,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, rhs, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QueryHetrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, off_diagonal, pivots, rhs, true);
}
Status Hetrs3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<float>> factors,
              DenseBlasVectorView<const std::complex<float>> off_diagonal,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, rhs, plan,
                 workspace, report, true);
}
Result<LapackWorkspacePlan> QueryHetrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, off_diagonal, pivots, rhs, true);
}
Status Hetrs3(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<double>> factors,
              DenseBlasVectorView<const std::complex<double>> off_diagonal,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, off_diagonal, pivots, rhs, plan,
                 workspace, report, true);
}
}  // namespace asc
