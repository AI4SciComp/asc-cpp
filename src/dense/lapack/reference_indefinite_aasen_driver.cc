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
#include "asc/dense/providers/lapack_indefinite_aasen_driver.h"
#include "internal_indefinite.h"
#include "internal_indefinite_aasen_driver_counts.h"
#include "internal_indefinite_calls.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace counts = internal_indefinite_aasen_driver_counts;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssysv_aa";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsysv_aa";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "chesv_aa" : "csysv_aa";
  } else {
    return he ? "zhesv_aa" : "zsysv_aa";
  }
}
template <typename T>
void Call(bool he, char triangle, lapack_int n, lapack_int nrhs, T* a,
          lapack_int lda, lapack_int* pivots, T* b, lapack_int ldb, T* work,
          lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssysv_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                    &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsysv_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                    &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chesv_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
    } else {
      LAPACK_csysv_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhesv_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
    } else {
      LAPACK_zsysv_aa(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
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
                                  DenseBlasVectorView<index_t> pivots,
                                  DenseBlasMatrixView<T> rhs, bool he) {
  if (!bk::Triangle(triangle) || pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || matrix.rows() != rhs.rows() ||
      matrix.rows() != pivots.size()) {
    return Status(ErrorCode::kShape);
  }
  if (matrix.leading_dimension() > bk::kIntegerLimit ||
      rhs.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{matrix.reachable_storage(), pivots.reachable_storage(),
                         rhs.reachable_storage(), bk::Object(provider)};
  for (const auto& status :
       {bk::Matrix(provider, matrix, he), bk::Matrix(provider, rhs),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Disjoint(spans),
        counts::Driver(matrix.rows(), rhs.columns(), bk::Leading(matrix, he),
                       RhsLeading(rhs), bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(he), bk::ScalarKind<T>(),
      std::array{matrix.rows(), rhs.columns(), bk::Leading(matrix, he),
                 RhsLeading(rhs), pivots.size()},
      std::array<std::int64_t, 7>{
          static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(he),
          static_cast<std::int64_t>(matrix.layout()),
          matrix.leading_dimension(), static_cast<std::int64_t>(rhs.layout()),
          rhs.leading_dimension(), pivots.increment()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  const auto n = matrix.rows();
  const auto preferred = counts::Preferred<DenseBlasRealType<T>>(
      n, DenseBlasComplex<T> && !he, bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  const auto minimum = std::max(2 * n, 3 * n - 2);
  plan.regions[bk::kScalar] = {minimum, *preferred, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
  for (const auto& status :
       {bk::Packing(matrix, plan, he), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}
bool ValidPivots(const lapack_int* pivots, lapack_int n) {
  if (pivots[0] != 1) {
    return false;
  }
  for (lapack_int i = 1; i < n; ++i) {
    if (pivots[i] < i + 1 || pivots[i] > n) {
      return false;
    }
  }
  return true;
}
template <typename T>
Status Publish(DenseBlasMatrixView<T> matrix,
               DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<T> rhs,
               DenseBlasTriangle triangle, bool he, const T* packed,
               const T* packed_rhs, const lapack_int* native_pivots,
               const T* work, lapack_int info, LapackReport& report) {
  if (info < 0 || info > matrix.rows() ||
      !ValidPivots(native_pivots, static_cast<lapack_int>(matrix.rows()))) {
    return bk::Defect(info, report);
  }
  if (info > 0) {
    // Final driver WORK(1) overwrites the N=1 GTSV diagonal witness.
    const auto diagonal =
        matrix.rows() == 1 ? packed[0] : work[matrix.rows() + info - 2];
    if (rhs.columns() == 0 || diagonal != T{}) {
      return bk::Defect(info, report);
    }
  }
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    pivots.data()[i] = native_pivots[i];
  }
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
               DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool he) {
  const std::array operands{matrix.reachable_storage(),
                            pivots.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(he), report);
  report.factor_family = LapackFactorFamily::kAasen;
  const auto expected = Query(provider, triangle, matrix, pivots, rhs, he);
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
  auto* native_pivots = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(n)];
  std::fill_n(native_pivots, n, std::numeric_limits<lapack_int>::min());
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(matrix, triangle, he, cursor, he);
  T dummy{};
  T* packed_rhs = rhs.columns() == 0 ? &dummy : bk::PackRhs(rhs, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  T sentinel{-1};
  if constexpr (DenseBlasComplex<T>) {
    sentinel.imag(-1);
  }
  work[0] = sentinel;
  std::fill_n(work + matrix.rows() - 1, matrix.rows(), sentinel);
  const auto entries = std::min<std::size_t>(
      workspace.regions[bk::kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(he, bk::Uplo(triangle), n, static_cast<lapack_int>(rhs.columns()),
       packed, static_cast<lapack_int>(bk::Leading(matrix, he)), native_pivots,
       packed_rhs, static_cast<lapack_int>(RhsLeading(rhs)), work,
       static_cast<lapack_int>(entries), info);
  report.native_info = info;
  if (work[0] != T{static_cast<DenseBlasRealType<T>>(
                     plan.regions[bk::kScalar].preferred_entries)}) {
    return bk::Defect(info, report);
  }
  return Publish(matrix, pivots, rhs, triangle, he, packed, packed_rhs,
                 native_pivots, work, info, report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}
Status SysvAa(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
              DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}
Status SysvAa(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
              DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
              const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}
Status SysvAa(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> matrix,
              DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}
Status SysvAa(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> matrix,
              DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}
Result<LapackWorkspacePlan> QueryHesvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, true);
}
Status HesvAa(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<float>> matrix,
              DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<std::complex<float>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, true);
}
Result<LapackWorkspacePlan> QueryHesvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, true);
}
Status HesvAa(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasMatrixView<std::complex<double>> matrix,
              DenseBlasVectorView<index_t> pivots,
              DenseBlasMatrixView<std::complex<double>> rhs,
              const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
              LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, true);
}
}  // namespace asc
