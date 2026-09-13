#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
#include "asc/dense/providers/lapack_indefinite_block_solve.h"
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
    return "ssytrs2";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsytrs2";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "chetrs2" : "csytrs2";
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    return hermitian ? "zhetrs2" : "zsytrs2";
  }
}
template <typename T>
void Call(bool hermitian, char triangle, lapack_int n, lapack_int nrhs,
          const T* a, lapack_int lda, const lapack_int* pivots, T* b,
          lapack_int ldb, T* work, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs2(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs2(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chetrs2(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                     &info);
    } else {
      LAPACK_csytrs2(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                     &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (hermitian) {
      LAPACK_zhetrs2(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                     &info);
    } else {
      LAPACK_zsytrs2(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                     &info);
    }
  }
}
Status PivotMetadata(const ReferenceLapackProvider& provider,
                     RawLapackPivotView pivots, extent_t order) {
  if (pivots.family() != LapackFactorFamily::kBunchKaufman) {
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
                                  RawLapackPivotView pivots,
                                  DenseBlasMatrixView<T> rhs, bool hermitian) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns() || factors.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{factors.reachable_storage(),
                         pivots.reachable_storage(), rhs.reachable_storage(),
                         bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, factors), bk::Matrix(provider, rhs),
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
      std::array{factors.rows(), rhs.columns(), bk::Leading(factors, true),
                 bk::Leading(rhs)},
      std::array<std::int64_t, 6>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(hermitian),
                                  static_cast<std::int64_t>(factors.layout()),
                                  factors.leading_dimension(),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.rows() == 0 || rhs.columns() == 0) {
    return plan;
  }
  const auto n = factors.rows();
  plan.regions[bk::kScalar] = {n, n, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  for (const Status& status :
       {bk::Packing(factors, plan, true), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}
// Byte identity is the input-restoration contract, distinct from floating-point
// equality: signed zeros and NaN payloads must survive conversion unchanged.
bool EqualBytes(const void* first, const void* second, std::size_t size) {
  return std::memcmp(first, second, size) == 0;
}

// Conversion and its inverse only move selected input scalars. Compare object
// bytes to retain signed zero and NaN payloads without numerical normalization.
template <typename T>
bool Restored(DenseBlasMatrixView<const T> factors, DenseBlasTriangle triangle,
              const T* packed) {
  for (extent_t j = 0; j < factors.columns(); ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : factors.rows();
    for (extent_t i = first; i < last; ++i) {
      if (!EqualBytes(&bk::Entry(factors, i, j),
                      &packed[j * factors.rows() + i], sizeof(T))) {
        return false;
      }
    }
  }
  return true;
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<const T> factors,
               RawLapackPivotView pivots, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  const auto expected =
      Query(provider, triangle, factors, pivots, rhs, hermitian);
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
  status = bk::Paired(pivots.values(), factors.rows(), triangle);
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots =
      internal_indefinite_expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* packed = bk::PackTriangle(factors, triangle, false, cursor, true);
  T* packed_rhs = bk::PackRhs(rhs, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(hermitian, bk::Uplo(triangle), static_cast<lapack_int>(factors.rows()),
       static_cast<lapack_int>(rhs.columns()), packed,
       static_cast<lapack_int>(bk::Leading(factors, true)), native_pivots,
       packed_rhs, static_cast<lapack_int>(bk::Leading(rhs)), work, info);
  report.native_info = info;
  if (info != 0 || !Restored(factors, triangle, packed)) {
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
Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sytrs2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const float> factors,
              RawLapackPivotView pivots, DenseBlasMatrixView<float> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sytrs2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const double> factors,
              RawLapackPivotView pivots, DenseBlasMatrixView<double> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sytrs2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<float>> factors,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status Sytrs2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<double>> factors,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QueryHetrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, true);
}
Status Hetrs2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<float>> factors,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, true);
}
Result<LapackWorkspacePlan> QueryHetrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, true);
}
Status Hetrs2(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<const std::complex<double>> factors,
              RawLapackPivotView pivots,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, true);
}
}  // namespace asc
