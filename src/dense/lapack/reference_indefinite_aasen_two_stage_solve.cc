#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.
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
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage_solve.h"
#include "internal_indefinite.h"
#include "internal_indefinite_aasen_two_stage_solve_counts.h"
#include "internal_indefinite_calls.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace counts = internal_indefinite_aasen_two_stage_solve_counts;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssytrs_aa_2stage";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsytrs_aa_2stage";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "chetrs_aa_2stage" : "csytrs_aa_2stage";
  } else {
    return he ? "zhetrs_aa_2stage" : "zsytrs_aa_2stage";
  }
}
template <typename T>
void Call(bool he, char triangle, lapack_int n, lapack_int nrhs, const T* a,
          lapack_int lda, const T* band, lapack_int ltb,
          const lapack_int* pivots, const lapack_int* band_pivots, T* b,
          lapack_int ldb, lapack_int& info) {
  // The pinned header lacks TB const, but all six sources and their GBTRS
  // closure read this persistent factor without modifying it.
  auto* tb = const_cast<T*>(band);
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrs_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, pivots,
                            band_pivots, b, &ldb, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrs_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, pivots,
                            band_pivots, b, &ldb, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrs_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, pivots,
                              band_pivots, b, &ldb, &info);
    } else {
      LAPACK_csytrs_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, pivots,
                              band_pivots, b, &ldb, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrs_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, pivots,
                              band_pivots, b, &ldb, &info);
    } else {
      LAPACK_zsytrs_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, pivots,
                              band_pivots, b, &ldb, &info);
    }
  }
}
Status PivotMetadata(const ReferenceLapackProvider& provider,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     extent_t n) {
  if (pivots.family() != LapackFactorFamily::kAasen ||
      band_pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (pivots.values().size() != static_cast<std::size_t>(n) ||
      band_pivots.size() != n) {
    return Status(ErrorCode::kShape);
  }
  for (const auto& status :
       {bk::Accessible(provider, pivots.reachable_storage()),
        bk::Accessible(provider, band_pivots.reachable_storage())}) {
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}
template <typename T>
Result<LapackWorkspacePlan> Query(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const T> factors, DenseBlasVectorView<const T> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<T> rhs, bool he) {
  if (!bk::Triangle(triangle) || band.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns() || factors.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (factors.leading_dimension() > bk::kIntegerLimit ||
      rhs.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{
      factors.reachable_storage(), band.reachable_storage(),
      pivots.reachable_storage(),  band_pivots.reachable_storage(),
      rhs.reachable_storage(),     bk::Object(provider)};
  for (const auto& status :
       {bk::Matrix(provider, factors), bk::Matrix(provider, rhs),
        bk::Accessible(provider, band.reachable_storage()),
        PivotMetadata(provider, pivots, band_pivots, factors.rows()),
        bk::Disjoint(spans),
        counts::Solve(factors.rows(), rhs.columns(), bk::Leading(factors),
                      band.size(), bk::Leading(rhs), bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(he), bk::ScalarKind<T>(),
      std::array{factors.rows(), rhs.columns(), bk::Leading(factors),
                 band.size(), bk::Leading(rhs),
                 static_cast<extent_t>(pivots.values().size()),
                 band_pivots.size()},
      std::array<std::int64_t, 10>{
          static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(he),
          static_cast<std::int64_t>(factors.layout()),
          factors.leading_dimension(), static_cast<std::int64_t>(rhs.layout()),
          rhs.leading_dimension(), static_cast<std::int64_t>(pivots.family()),
          band.increment(), band_pivots.increment(), 2},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.rows() == 0 || rhs.columns() == 0) {
    return plan;
  }
  const auto n = factors.rows();
  plan.regions[bk::kPivot] = {2 * n, 2 * n, sizeof(lapack_int),
                              alignof(lapack_int)};
  for (const auto& status :
       {bk::Packing(factors, plan), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}
template <typename T>
Result<extent_t> BlockWidth(DenseBlasVectorView<const T> band, extent_t n) {
  const auto maximum = std::min<extent_t>(192, (band.size() / n - 1) / 3);
  const auto value = std::real(band.data()[0]);
  // Bounds reject NaN/infinity before the INTEGER conversion. Exact scalar
  // equality checks an encoded integer, not an approximate numerical result.
  if (!(value >= 1 && value <= maximum)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto nb = static_cast<extent_t>(value);
  if (band.data()[0] != T{static_cast<DenseBlasRealType<T>>(nb)}) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return nb;
}
Status PivotValues(RawLapackPivotView pivots,
                   DenseBlasVectorView<const index_t> band_pivots, extent_t n,
                   extent_t nb) {
  const auto p = pivots.values();
  const auto* q = band_pivots.data();
  for (extent_t i = 0; i < n; ++i) {
    if (p[i] < i + 1 || p[i] > n || (i < nb && p[i] != i + 1) || q[i] < i + 1 ||
        q[i] > i + 1 + std::min(nb, n - i - 1)) {
      return Status(ErrorCode::kInvalidArgument);
    }
  }
  return Status::Ok();
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<const T> factors,
               DenseBlasVectorView<const T> band, RawLapackPivotView pivots,
               DenseBlasVectorView<const index_t> band_pivots,
               DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool he) {
  const std::array operands{
      factors.reachable_storage(), band.reachable_storage(),
      pivots.reachable_storage(), band_pivots.reachable_storage(),
      rhs.reachable_storage()};
  auto status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(he), report);
  report.factor_family = LapackFactorFamily::kAasen;
  const auto expected =
      Query(provider, triangle, factors, band, pivots, band_pivots, rhs, he);
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
  const auto block = BlockWidth(band, factors.rows());
  if (!block.ok()) {
    return block.status();
  }
  status = PivotValues(pivots, band_pivots, factors.rows(), *block);
  if (!status.ok()) {
    return status;
  }
  const auto ldtb = band.size() / factors.rows();
  for (extent_t j = 0; j < factors.rows(); ++j) {
    if (band.data()[j * ldtb + 2 * (*block)] == T{}) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index = j;
      return Status(ErrorCode::kNumerical);
    }
  }
  const auto n = static_cast<lapack_int>(factors.rows());
  auto* native = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(2 * n)];
  for (lapack_int i = 0; i < n; ++i) {
    native[i] = static_cast<lapack_int>(pivots.values()[i]);
    native[n + i] = static_cast<lapack_int>(band_pivots.data()[i]);
  }
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* packed = bk::PackTriangle(factors, triangle, false, cursor);
  T* packed_rhs = bk::PackRhs(rhs, cursor);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(he, bk::Uplo(triangle), n, static_cast<lapack_int>(rhs.columns()),
       packed, static_cast<lapack_int>(bk::Leading(factors)), band.data(),
       static_cast<lapack_int>(band.size()), native, native + n, packed_rhs,
       static_cast<lapack_int>(bk::Leading(rhs)), info);
  report.native_info = info;
  if (info != 0) {
    return bk::Defect(info, report);
  }
  for (lapack_int i = 0; i < n; ++i) {
    if (native[i] != pivots.values()[i] ||
        native[n + i] != band_pivots.data()[i]) {
      return bk::Defect(info, report);
    }
  }
  bk::PublishRhs(packed_rhs, rhs);
  return bk::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySytrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors,
    DenseBlasVectorView<const float> band, RawLapackPivotView pivots,
    DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, factors, band, pivots, band_pivots, rhs,
               false);
}
Status SytrsAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const float> factors,
                     DenseBlasVectorView<const float> band,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     DenseBlasMatrixView<float> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> band, RawLapackPivotView pivots,
    DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, factors, band, pivots, band_pivots, rhs,
               false);
}
Status SytrsAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const double> factors,
                     DenseBlasVectorView<const double> band,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     DenseBlasMatrixView<double> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, band, pivots, band_pivots, rhs,
               false);
}
Status SytrsAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<float>> factors,
                     DenseBlasVectorView<const std::complex<float>> band,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     DenseBlasMatrixView<std::complex<float>> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, band, pivots, band_pivots, rhs,
               false);
}
Status SytrsAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<double>> factors,
                     DenseBlasVectorView<const std::complex<double>> band,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     DenseBlasMatrixView<std::complex<double>> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QueryHetrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, factors, band, pivots, band_pivots, rhs,
               true);
}
Status HetrsAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<float>> factors,
                     DenseBlasVectorView<const std::complex<float>> band,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     DenseBlasMatrixView<std::complex<float>> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, band, pivots, band_pivots, rhs,
                 plan, workspace, report, true);
}
Result<LapackWorkspacePlan> QueryHetrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, factors, band, pivots, band_pivots, rhs,
               true);
}
Status HetrsAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<const std::complex<double>> factors,
                     DenseBlasVectorView<const std::complex<double>> band,
                     RawLapackPivotView pivots,
                     DenseBlasVectorView<const index_t> band_pivots,
                     DenseBlasMatrixView<std::complex<double>> rhs,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, band, pivots, band_pivots, rhs,
                 plan, workspace, report, true);
}
}  // namespace asc
