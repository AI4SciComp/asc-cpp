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
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage_driver.h"
#include "internal_indefinite.h"
#include "internal_indefinite_aasen_two_stage_counts.h"
#include "internal_indefinite_aasen_two_stage_driver_counts.h"
#include "internal_indefinite_calls.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace counts = internal_indefinite_aasen_two_stage_driver_counts;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssysv_aa_2stage";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsysv_aa_2stage";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "chesv_aa_2stage" : "csysv_aa_2stage";
  } else {
    return he ? "zhesv_aa_2stage" : "zsysv_aa_2stage";
  }
}
template <typename T>
void Call(bool he, char triangle, lapack_int n, lapack_int nrhs, T* a,
          lapack_int lda, T* tb, lapack_int ltb, lapack_int* p, lapack_int* q,
          T* b, lapack_int ldb, T* work, lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssysv_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, p, q, b,
                           &ldb, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsysv_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, p, q, b,
                           &ldb, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chesv_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, p, q, b,
                             &ldb, work, &lwork, &info);
    } else {
      LAPACK_csysv_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, p, q, b,
                             &ldb, work, &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhesv_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, p, q, b,
                             &ldb, work, &lwork, &info);
    } else {
      LAPACK_zsysv_aa_2stage(&triangle, &n, &nrhs, a, &lda, tb, &ltb, p, q, b,
                             &ldb, work, &lwork, &info);
    }
  }
}
template <typename T>
extent_t RhsLeading(DenseBlasMatrixView<T> rhs) {
  return rhs.columns() == 0 ? std::max<extent_t>(1, rhs.rows())
                            : bk::Leading(rhs);
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<T> matrix,
                                  DenseBlasVectorView<T> band,
                                  DenseBlasVectorView<index_t> pivots,
                                  DenseBlasVectorView<index_t> band_pivots,
                                  DenseBlasMatrixView<T> rhs, bool he) {
  if (!bk::Triangle(triangle) || band.increment() != 1 ||
      pivots.increment() != 1 || band_pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || matrix.rows() != pivots.size() ||
      matrix.rows() != band_pivots.size() || matrix.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (matrix.leading_dimension() > bk::kIntegerLimit ||
      rhs.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{
      matrix.reachable_storage(), band.reachable_storage(),
      pivots.reachable_storage(), band_pivots.reachable_storage(),
      rhs.reachable_storage(),    bk::Object(provider)};
  for (const auto& status :
       {bk::Matrix(provider, matrix, he), bk::Matrix(provider, rhs),
        bk::Accessible(provider, band.reachable_storage()),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Accessible(provider, band_pivots.reachable_storage()),
        bk::Disjoint(spans),
        counts::Driver(matrix.rows(), rhs.columns(), bk::Leading(matrix, he),
                       band.size(), RhsLeading(rhs), bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(he), bk::ScalarKind<T>(),
      std::array{matrix.rows(), rhs.columns(), bk::Leading(matrix, he),
                 band.size(), pivots.size(), band_pivots.size(),
                 RhsLeading(rhs)},
      std::array<std::int64_t, 10>{
          static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(he),
          static_cast<std::int64_t>(matrix.layout()),
          matrix.leading_dimension(), static_cast<std::int64_t>(rhs.layout()),
          rhs.leading_dimension(), band.increment(), pivots.increment(),
          band_pivots.increment(), 2},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  const auto n = matrix.rows();
  const auto preferred =
      counts::Preferred<DenseBlasRealType<T>>(n, bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  plan.regions[bk::kScalar] = {n, *preferred, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {2 * n, 2 * n, sizeof(lapack_int),
                              alignof(lapack_int)};
  for (const auto& status :
       {bk::Packing(matrix, plan, he), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}
bool ValidPivots(const lapack_int* p, const lapack_int* q, lapack_int n,
                 lapack_int nb) {
  for (lapack_int i = 0; i < n; ++i) {
    if (p[i] < i + 1 || p[i] > n || (i < nb && p[i] != i + 1) || q[i] < i + 1 ||
        q[i] > i + 1 + std::min(nb, n - i - 1)) {
      return false;
    }
  }
  return true;
}
template <typename T>
Status Publish(DenseBlasMatrixView<T> matrix, DenseBlasVectorView<T> band,
               DenseBlasVectorView<index_t> pivots,
               DenseBlasVectorView<index_t> band_pivots,
               DenseBlasMatrixView<T> rhs, DenseBlasTriangle triangle, bool he,
               const T* packed, const T* packed_rhs, const lapack_int* native,
               const T* work, lapack_int nb, lapack_int info,
               const LapackWorkspacePlan& plan, LapackReport& report) {
  const auto n = static_cast<lapack_int>(matrix.rows());
  const auto ldtb = band.size() / n;
  if (info < 0 || info > n ||
      band.data()[0] != T{static_cast<DenseBlasRealType<T>>(nb)} ||
      !ValidPivots(native, native + n, n, nb) ||
      work[0] != T{static_cast<DenseBlasRealType<T>>(
                     plan.regions[bk::kScalar].preferred_entries)} ||
      (info > 0 && band.data()[(info - 1) * ldtb + 2 * nb] != T{})) {
    return bk::Defect(info, report);
  }
  std::copy_n(native, n, pivots.data());
  std::copy_n(native + n, n, band_pivots.data());
  bk::PublishTriangle(packed, matrix, triangle, he);
  if (info == 0) {
    bk::PublishRhs(packed_rhs, rhs);
    return bk::Complete(report);
  }
  report.diagnostic_index = info - 1;
  report.outcome = LapackOutcome::kSingular;
  report.output_validity = LapackOutputValidity::kDocumentedPartial;
  return Status(ErrorCode::kNumerical);
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
               DenseBlasVectorView<T> band, DenseBlasVectorView<index_t> pivots,
               DenseBlasVectorView<index_t> band_pivots,
               DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool he) {
  const std::array operands{
      matrix.reachable_storage(), band.reachable_storage(),
      pivots.reachable_storage(), band_pivots.reachable_storage(),
      rhs.reachable_storage()};
  auto status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(he), report);
  report.factor_family = LapackFactorFamily::kAasen;
  const auto expected =
      Query(provider, triangle, matrix, band, pivots, band_pivots, rhs, he);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows() == 0) {
    return bk::Complete(report);
  }
  const auto n = static_cast<lapack_int>(matrix.rows());
  const auto entries = std::min<std::size_t>(
      workspace.regions[bk::kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
  const auto nb = static_cast<lapack_int>(
      internal_indefinite_aasen_two_stage_counts::BlockWidth(n, band.size(),
                                                             entries));
  const auto ldtb = band.size() / n;
  auto* native = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(2 * n)];
  std::fill_n(native, 2 * n, std::numeric_limits<lapack_int>::min());
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(matrix, triangle, he, cursor, he);
  T dummy{};
  T* packed_rhs = rhs.columns() == 0 ? &dummy : bk::PackRhs(rhs, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  T sentinel{-1};
  if constexpr (DenseBlasComplex<T>) {
    sentinel.imag(-1);
  }
  band.data()[0] = sentinel;
  for (lapack_int j = 0; j < n; ++j) {
    band.data()[j * ldtb + 2 * nb] = sentinel;
  }
  work[0] = sentinel;
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(he, bk::Uplo(triangle), n, static_cast<lapack_int>(rhs.columns()),
       packed, static_cast<lapack_int>(bk::Leading(matrix, he)), band.data(),
       static_cast<lapack_int>(band.size()), native, native + n, packed_rhs,
       static_cast<lapack_int>(RhsLeading(rhs)), work,
       static_cast<lapack_int>(entries), info);
  report.native_info = info;
  return Publish(matrix, band, pivots, band_pivots, rhs, triangle, he, packed,
                 packed_rhs, native, work, nb, info, plan, report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySysvAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, rhs,
               false);
}
Status SysvAa2Stage(const ReferenceLapackProvider& provider,
                    DenseBlasTriangle triangle,
                    DenseBlasMatrixView<float> matrix,
                    DenseBlasVectorView<float> band,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasVectorView<index_t> band_pivots,
                    DenseBlasMatrixView<float> rhs,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySysvAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<double> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, rhs,
               false);
}
Status SysvAa2Stage(const ReferenceLapackProvider& provider,
                    DenseBlasTriangle triangle,
                    DenseBlasMatrixView<double> matrix,
                    DenseBlasVectorView<double> band,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasVectorView<index_t> band_pivots,
                    DenseBlasMatrixView<double> rhs,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySysvAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, rhs,
               false);
}
Status SysvAa2Stage(const ReferenceLapackProvider& provider,
                    DenseBlasTriangle triangle,
                    DenseBlasMatrixView<std::complex<float>> matrix,
                    DenseBlasVectorView<std::complex<float>> band,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasVectorView<index_t> band_pivots,
                    DenseBlasMatrixView<std::complex<float>> rhs,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySysvAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, rhs,
               false);
}
Status SysvAa2Stage(const ReferenceLapackProvider& provider,
                    DenseBlasTriangle triangle,
                    DenseBlasMatrixView<std::complex<double>> matrix,
                    DenseBlasVectorView<std::complex<double>> band,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasVectorView<index_t> band_pivots,
                    DenseBlasMatrixView<std::complex<double>> rhs,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, rhs,
                 plan, workspace, report, false);
}
Result<LapackWorkspacePlan> QueryHesvAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, rhs,
               true);
}
Status HesvAa2Stage(const ReferenceLapackProvider& provider,
                    DenseBlasTriangle triangle,
                    DenseBlasMatrixView<std::complex<float>> matrix,
                    DenseBlasVectorView<std::complex<float>> band,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasVectorView<index_t> band_pivots,
                    DenseBlasMatrixView<std::complex<float>> rhs,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, rhs,
                 plan, workspace, report, true);
}
Result<LapackWorkspacePlan> QueryHesvAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, rhs,
               true);
}
Status HesvAa2Stage(const ReferenceLapackProvider& provider,
                    DenseBlasTriangle triangle,
                    DenseBlasMatrixView<std::complex<double>> matrix,
                    DenseBlasVectorView<std::complex<double>> band,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasVectorView<index_t> band_pivots,
                    DenseBlasMatrixView<std::complex<double>> rhs,
                    const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, rhs,
                 plan, workspace, report, true);
}
}  // namespace asc
