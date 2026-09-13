#include <algorithm>
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
#include "asc/dense/providers/lapack_indefinite_aasen_solve.h"
#include "internal_indefinite.h"
#include "internal_indefinite_aasen_solve_counts.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace counts = internal_indefinite_aasen_solve_counts;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssytrs_aa";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsytrs_aa";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "chetrs_aa" : "csytrs_aa";
  } else {
    return he ? "zhetrs_aa" : "zsytrs_aa";
  }
}
template <typename T>
void Call(bool he, char triangle, lapack_int n, lapack_int nrhs, const T* a,
          lapack_int lda, const lapack_int* pivots, T* b, lapack_int ldb,
          T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                     &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                     &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrs_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                       &lwork, &info);
    } else {
      LAPACK_csytrs_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                       &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrs_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                       &lwork, &info);
    } else {
      LAPACK_zsytrs_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                       &lwork, &info);
    }
  }
}
Status PivotMetadata(const ReferenceLapackProvider& provider,
                     RawLapackPivotView pivots, extent_t n) {
  if (pivots.family() != LapackFactorFamily::kAasen) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (pivots.values().size() != static_cast<std::size_t>(n)) {
    return Status(ErrorCode::kShape);
  }
  return bk::Accessible(provider, pivots.reachable_storage());
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<const T> factors,
                                  RawLapackPivotView pivots,
                                  DenseBlasMatrixView<T> rhs, bool he) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns() || factors.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (factors.leading_dimension() > bk::kIntegerLimit ||
      rhs.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{factors.reachable_storage(),
                         pivots.reachable_storage(), rhs.reachable_storage(),
                         bk::Object(provider)};
  for (const auto& status :
       {bk::Matrix(provider, factors), bk::Matrix(provider, rhs),
        PivotMetadata(provider, pivots, factors.rows()), bk::Disjoint(spans),
        counts::Solve(factors.rows(), rhs.columns(), bk::Leading(factors),
                      bk::Leading(rhs), bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(he), bk::ScalarKind<T>(),
      std::array{factors.rows(), rhs.columns(), bk::Leading(factors),
                 bk::Leading(rhs),
                 static_cast<extent_t>(pivots.values().size())},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(he),
          static_cast<std::int64_t>(factors.layout()),
          factors.leading_dimension(), static_cast<std::int64_t>(rhs.layout()),
          rhs.leading_dimension(), static_cast<std::int64_t>(pivots.family())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.rows() == 0 || rhs.columns() == 0) {
    return plan;
  }
  const auto n = factors.rows();
  const auto preferred =
      counts::Preferred<DenseBlasRealType<T>>(n, bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  plan.regions[bk::kScalar] = {3 * n - 2, *preferred, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  for (const auto& status :
       {bk::Packing(factors, plan), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}
Status PivotValues(RawLapackPivotView pivots, extent_t n) {
  const auto values = pivots.values();
  if (values[0] != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (extent_t i = 1; i < n; ++i) {
    const auto p = values[static_cast<std::size_t>(i)];
    if (p < i + 1 || p > n) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<const T> factors,
               RawLapackPivotView pivots, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool he) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(he), report);
  report.factor_family = LapackFactorFamily::kAasen;
  const auto expected = Query(provider, triangle, factors, pivots, rhs, he);
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
  status = PivotValues(pivots, factors.rows());
  if (!status.ok()) {
    return status;
  }
  auto* native_pivots =
      internal_indefinite_expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* packed = bk::PackTriangle(factors, triangle, false, cursor);
  T* packed_rhs = bk::PackRhs(rhs, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  T sentinel{-1};
  if constexpr (DenseBlasComplex<T>) {
    sentinel.imag(-1);
  }
  std::fill_n(work + factors.rows() - 1, factors.rows(), sentinel);
  const auto entries = std::min<std::size_t>(
      workspace.regions[bk::kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(he, bk::Uplo(triangle), static_cast<lapack_int>(factors.rows()),
       static_cast<lapack_int>(rhs.columns()), packed,
       static_cast<lapack_int>(bk::Leading(factors)), native_pivots, packed_rhs,
       static_cast<lapack_int>(bk::Leading(rhs)), work,
       static_cast<lapack_int>(entries), info);
  report.native_info = info;
  if (info < 0 || info > factors.rows()) {
    return bk::Defect(info, report);
  }
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  if (info > 0) {
    if (work[factors.rows() + info - 2] != T{}) {
      return bk::Defect(info, report);
    }
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kUnusable;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  bk::PublishRhs(packed_rhs, rhs);
  return bk::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status SytrsAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const float> factors,
               RawLapackPivotView pivots, DenseBlasMatrixView<float> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status SytrsAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const double> factors,
               RawLapackPivotView pivots, DenseBlasMatrixView<double> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status SytrsAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const std::complex<float>> factors,
               RawLapackPivotView pivots,
               DenseBlasMatrixView<std::complex<float>> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, false);
}
Status SytrsAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const std::complex<double>> factors,
               RawLapackPivotView pivots,
               DenseBlasMatrixView<std::complex<double>> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QueryHetrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, true);
}
Status HetrsAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const std::complex<float>> factors,
               RawLapackPivotView pivots,
               DenseBlasMatrixView<std::complex<float>> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, true);
}
Result<LapackWorkspacePlan> QueryHetrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, pivots, rhs, true);
}
Status HetrsAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const std::complex<double>> factors,
               RawLapackPivotView pivots,
               DenseBlasMatrixView<std::complex<double>> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, rhs, plan, workspace,
                 report, true);
}
}  // namespace asc
