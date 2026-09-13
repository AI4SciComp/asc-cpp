#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; explicit caller integer lifetimes.
#include <span>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook_driver.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_counts.h"
#include "internal_indefinite_expert_counts.h"
#include "internal_indefinite_rook_origin.h"
#include "internal_indefinite_rook_pivots.h"

namespace asc {
namespace {
namespace bk = internal_indefinite;

namespace rook = internal_indefinite_rook;

template <typename T>
extent_t RhsLeading(DenseBlasMatrixView<T> rhs) {
  // N>0, NRHS=0 still enters SV and its scalar LDB check. The matrix is
  // unread, but LDB must be at least N; a valid local dummy is passed below.
  return rhs.columns() == 0 ? std::max<extent_t>(1, rhs.rows())
                            : bk::Leading(rhs);
}

void Call([[maybe_unused]] bool hermitian, char triangle, lapack_int n,
          lapack_int nrhs, float* a, lapack_int lda, lapack_int* pivots,
          float* b, lapack_int ldb, float* work, lapack_int lwork,
          lapack_int& info) {
  LAPACK_ssysv_rook(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                    &lwork, &info);
}

void Call([[maybe_unused]] bool hermitian, char triangle, lapack_int n,
          lapack_int nrhs, double* a, lapack_int lda, lapack_int* pivots,
          double* b, lapack_int ldb, double* work, lapack_int lwork,
          lapack_int& info) {
  LAPACK_dsysv_rook(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                    &lwork, &info);
}

void Call(bool hermitian, char triangle, lapack_int n, lapack_int nrhs,
          std::complex<float>* a, lapack_int lda, lapack_int* pivots,
          std::complex<float>* b, lapack_int ldb, std::complex<float>* work,
          lapack_int lwork, lapack_int& info) {
  if (hermitian) {
    LAPACK_chesv_rook(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
  } else {
    LAPACK_csysv_rook(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
  }
}

void Call(bool hermitian, char triangle, lapack_int n, lapack_int nrhs,
          std::complex<double>* a, lapack_int lda, lapack_int* pivots,
          std::complex<double>* b, lapack_int ldb, std::complex<double>* work,
          lapack_int lwork, lapack_int& info) {
  if (hermitian) {
    LAPACK_zhesv_rook(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
  } else {
    LAPACK_zsysv_rook(&triangle, &n, &nrhs, a, &lda, pivots, b, &ldb, work,
                      &lwork, &info);
  }
}

template <typename T>
Status Metadata(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
                DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<T> rhs,
                bool hermitian) {
  if (!bk::Triangle(triangle) || pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || pivots.size() != matrix.rows() ||
      rhs.rows() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{matrix.reachable_storage(), pivots.reachable_storage(),
                         rhs.reachable_storage(), bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, matrix, hermitian), bk::Matrix(provider, rhs),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Disjoint(spans),
        internal_indefinite_counts::Factor(
            matrix.rows(), bk::Leading(matrix, hermitian), bk::kIntegerLimit),
        internal_indefinite_counts::Solve(matrix.rows(), rhs.columns(),
                                          RhsLeading(rhs),
                                          bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return Status::Ok();
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<T> matrix,
                                  DenseBlasVectorView<index_t> pivots,
                                  DenseBlasMatrixView<T> rhs, bool hermitian) {
  Status status = Metadata(provider, triangle, matrix, pivots, rhs, hermitian);
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      rook::DriverName<T>(hermitian), bk::ScalarKind<T>(),
      std::array{matrix.rows(), rhs.columns(), bk::Leading(matrix, hermitian),
                 RhsLeading(rhs), pivots.size()},
      std::array<std::int64_t, 6>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(hermitian),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension(),
                                  static_cast<std::int64_t>(rhs.layout()),
                                  rhs.leading_dimension()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  const auto preferred =
      internal_indefinite_expert_counts::Driver<DenseBlasRealType<T>>(
          matrix.rows(), bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  plan.regions[bk::kScalar] = {1, *preferred, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {matrix.rows(), matrix.rows(), sizeof(lapack_int),
                              alignof(lapack_int)};
  for (const Status& packing :
       {bk::Packing(matrix, plan, hermitian), bk::Packing(rhs, plan)}) {
    if (!packing.ok()) {
      return packing;
    }
  }
  return plan;
}

template <typename T>
Status Publish(DenseBlasMatrixView<T> matrix,
               DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<T> rhs,
               DenseBlasTriangle triangle, bool hermitian, const T* packed,
               const T* packed_rhs, const lapack_int* native_pivots,
               lapack_int info, LapackReport& report) {
  if (info < 0 || info > matrix.rows()) {
    return bk::Defect(info, report);
  }
  const auto native = std::span<const lapack_int>(
      native_pivots, static_cast<std::size_t>(matrix.rows()));
  if (!rook::Paired(native, matrix.rows(), triangle).ok()) {
    return bk::Defect(info, report);
  }
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    pivots.data()[i] = native_pivots[i];
  }
  bk::PublishTriangle(packed, matrix, triangle, hermitian);
  if (info == 0) {
    bk::PublishRhs(packed_rhs, rhs);
    return bk::Complete(report);
  }
  report.diagnostic_index = static_cast<index_t>(info) - 1;
  report.outcome = bk::Entry(matrix, info - 1, info - 1) == T{}
                       ? LapackOutcome::kSingular
                       : LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kDocumentedPartial;
  return Status(ErrorCode::kNumerical);
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
               DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<T> rhs,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{matrix.reachable_storage(),
                            pivots.reachable_storage(),
                            rhs.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, rook::DriverName<T>(hermitian), report);
  report.factor_family = LapackFactorFamily::kRook;
  const auto expected =
      Query(provider, triangle, matrix, pivots, rhs, hermitian);
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
  // Every provider-width output is seeded after all structural checks.
  std::fill_n(native_pivots, n, std::numeric_limits<lapack_int>::min());
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(matrix, triangle, hermitian, cursor, hermitian);
  T dummy{};
  T* packed_rhs = rhs.columns() == 0 ? &dummy : bk::PackRhs(rhs, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  const auto entries = std::min<std::size_t>(
      workspace.regions[bk::kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
  work[0] = T{-1};
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(hermitian, bk::Uplo(triangle), n, static_cast<lapack_int>(rhs.columns()),
       packed, static_cast<lapack_int>(bk::Leading(matrix, hermitian)),
       native_pivots, packed_rhs, static_cast<lapack_int>(RhsLeading(rhs)),
       work, static_cast<lapack_int>(entries), info);
  report.native_info = info;
  if (info >= 0 &&
      work[0] != T{static_cast<DenseBlasRealType<T>>(
                     plan.regions[bk::kScalar].preferred_entries)}) {
    return bk::Defect(info, report);
  }
  return Publish(matrix, pivots, rhs, triangle, hermitian, packed, packed_rhs,
                 native_pivots, info, report);
}
}  // namespace

Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<float> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}

Status SysvRook(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
                DenseBlasVectorView<index_t> pivots,
                DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<double> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}

Status SysvRook(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
                DenseBlasVectorView<index_t> pivots,
                DenseBlasMatrixView<double> rhs,
                const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}

Status SysvRook(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle,
                DenseBlasMatrixView<std::complex<float>> matrix,
                DenseBlasVectorView<index_t> pivots,
                DenseBlasMatrixView<std::complex<float>> rhs,
                const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, false);
}

Status SysvRook(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle,
                DenseBlasMatrixView<std::complex<double>> matrix,
                DenseBlasVectorView<index_t> pivots,
                DenseBlasMatrixView<std::complex<double>> rhs,
                const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, false);
}

Result<LapackWorkspacePlan> QueryHesvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, true);
}

Status HesvRook(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle,
                DenseBlasMatrixView<std::complex<float>> matrix,
                DenseBlasVectorView<index_t> pivots,
                DenseBlasMatrixView<std::complex<float>> rhs,
                const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, true);
}

Result<LapackWorkspacePlan> QueryHesvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return Query(provider, triangle, matrix, pivots, rhs, true);
}

Status HesvRook(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle,
                DenseBlasMatrixView<std::complex<double>> matrix,
                DenseBlasVectorView<index_t> pivots,
                DenseBlasMatrixView<std::complex<double>> rhs,
                const LapackWorkspacePlan& plan,
                const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, rhs, plan, workspace,
                 report, true);
}

}  // namespace asc
