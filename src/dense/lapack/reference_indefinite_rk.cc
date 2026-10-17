#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; caller-owned foreign integer lifetimes.
#include <span>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk.h"
#include "internal_indefinite.h"
#include "internal_indefinite_counts.h"
#include "internal_indefinite_rk_calls.h"

namespace asc {
namespace {

namespace bk = internal_indefinite;
namespace rk = internal_indefinite_rk;

// RK stores explicit global permutations. Each negative partner has its own
// directional target, and its structural-zero E entry is opposite its block
// offdiagonal. Validate source storage without imposing a finiteness policy.
template <typename T>
Status NativeStorage(std::span<const lapack_int> pivots, const T* off_diagonal,
                     const T* matrix, lapack_int lda,
                     DenseBlasTriangle triangle) {
  const auto n = static_cast<extent_t>(pivots.size());
  const bool upper = triangle == DenseBlasTriangle::kUpper;
  for (extent_t i = 0; i < n;) {
    const auto p = pivots[static_cast<std::size_t>(i)];
    if (p == 0 || p < -n || p > n) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto target = p < 0 ? -p : p;
    if ((upper && target > i + 1) || (!upper && target < i + 1)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (p > 0) {
      if (off_diagonal[i] != T{}) {
        return Status(ErrorCode::kInvalidArgument);
      }
      ++i;
      continue;
    }
    if (i + 1 >= n) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto q = pivots[static_cast<std::size_t>(i + 1)];
    if (q >= 0 || q < -n || (upper && -q > i + 2) || (!upper && -q < i + 2)) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const auto zero = upper ? i : i + 1;
    const auto offset = upper ? (i + 1) * lda + i : i * lda + i + 1;
    if (off_diagonal[zero] != T{} || matrix[offset] != T{}) {
      return Status(ErrorCode::kInvalidArgument);
    }
    i += 2;
  }
  return Status::Ok();
}

template <typename T>
Status FactorMetadata(const ReferenceLapackProvider& provider,
                      DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
                      DenseBlasVectorView<T> off_diagonal,
                      DenseBlasVectorView<index_t> pivots, bool hermitian) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || pivots.size() != matrix.rows() ||
      off_diagonal.size() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (pivots.increment() != 1 || off_diagonal.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array spans{matrix.reachable_storage(),
                         off_diagonal.reachable_storage(),
                         pivots.reachable_storage(), bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, matrix, hermitian),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Accessible(provider, off_diagonal.reachable_storage()),
        bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (matrix.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  return internal_indefinite_counts::Factor(
      matrix.rows(), bk::Leading(matrix, hermitian), bk::kIntegerLimit);
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        DenseBlasTriangle triangle,
                                        DenseBlasMatrixView<T> matrix,
                                        DenseBlasVectorView<T> off_diagonal,
                                        DenseBlasVectorView<index_t> pivots,
                                        rk::Routine routine, bool hermitian) {
  Status metadata = FactorMetadata(provider, triangle, matrix, off_diagonal,
                                   pivots, hermitian);
  if (!metadata.ok()) {
    return metadata;
  }
  const std::array dimensions{matrix.rows(), matrix.columns(),
                              bk::Leading(matrix, hermitian),
                              off_diagonal.size(), pivots.size()};
  const std::array<std::int64_t, 6> options{
      static_cast<std::int64_t>(triangle),
      static_cast<std::int64_t>(hermitian),
      static_cast<std::int64_t>(matrix.layout()),
      matrix.leading_dimension(),
      off_diagonal.increment(),
      pivots.increment()};
  const auto identity = LapackPlanIdentity::Create(
      rk::Name<T>(routine, hermitian), rk::ScalarKind<T>(), dimensions, options,
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  if (routine == rk::Routine::kTrf) {
    const auto preferred =
        internal_indefinite_counts::Preferred<DenseBlasRealType<T>>(
            matrix.rows(), bk::kIntegerLimit);
    if (!preferred.ok()) {
      return preferred.status();
    }
    plan.regions[bk::kScalar] = {1, *preferred, sizeof(T), alignof(T)};
  }
  plan.regions[bk::kPivot] = {matrix.rows(), matrix.rows(), sizeof(lapack_int),
                              alignof(lapack_int)};
  Status packing = bk::Packing(matrix, plan, hermitian);
  if (!packing.ok()) {
    return packing;
  }
  return plan;
}

template <typename T>
Status FinishFactor(DenseBlasMatrixView<T> matrix,
                    DenseBlasVectorView<T> off_diagonal,
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasTriangle triangle, bool hermitian, const T* packed,
                    const lapack_int* native_pivots, lapack_int info,
                    LapackReport& report) {
  if (info < 0 || info > matrix.rows()) {
    return bk::Defect(info, report);
  }
  const auto native_span = std::span<const lapack_int>(
      native_pivots, static_cast<std::size_t>(matrix.rows()));
  if (!NativeStorage(native_span, off_diagonal.data(), packed,
                     static_cast<lapack_int>(bk::Leading(matrix, hermitian)),
                     triangle)
           .ok()) {
    return bk::Defect(info, report);
  }
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    pivots.data()[i] = native_pivots[i];
  }
  bk::PublishTriangle(packed, matrix, triangle, hermitian);
  report.native_info = info;
  if (info == 0) {
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
Status ExecuteFactor(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
                     DenseBlasVectorView<T> off_diagonal,
                     DenseBlasVectorView<index_t> pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report,
                     rk::Routine routine, bool hermitian) {
  const std::array operands{matrix.reachable_storage(),
                            off_diagonal.reachable_storage(),
                            pivots.reachable_storage()};
  Status metadata = bk::Metadata(provider, plan, workspace, report, operands);
  if (!metadata.ok()) {
    return metadata;
  }
  bk::Start(provider, rk::Name<T>(routine, hermitian), report);
  report.factor_family = LapackFactorFamily::kRook;
  const auto expected = QueryFactor(provider, triangle, matrix, off_diagonal,
                                    pivots, routine, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  Status valid = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!valid.ok()) {
    return valid;
  }
  if (matrix.rows() == 0) {
    return bk::Complete(report);
  }
  const auto n = static_cast<lapack_int>(matrix.rows());
  const auto lda = static_cast<lapack_int>(bk::Leading(matrix, hermitian));
  auto* native_pivots = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(n)]{};
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(matrix, triangle, hermitian, cursor, hermitian);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  if (routine == rk::Routine::kTrf) {
    auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
    const auto entries = std::min<std::size_t>(
        workspace.regions[bk::kScalar].size() / sizeof(T),
        static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
    rk::TrfCall(hermitian, bk::Uplo(triangle), n, packed, lda,
                off_diagonal.data(), native_pivots, work,
                static_cast<lapack_int>(entries), info);
    if (info >= 0 &&
        work[0] != T{static_cast<DenseBlasRealType<T>>(
                       plan.regions[bk::kScalar].preferred_entries)}) {
      return bk::Defect(info, report);
    }
  } else {
    rk::Tf2Call(hermitian, bk::Uplo(triangle), n, packed, lda,
                off_diagonal.data(), native_pivots, info);
  }
  return FinishFactor(matrix, off_diagonal, pivots, triangle, hermitian, packed,
                      native_pivots, info, report);
}

}  // namespace

Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTf2, false);
}

Status Sytf2Rk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
               DenseBlasVectorView<float> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTf2, false);
}

Status Sytf2Rk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
               DenseBlasVectorView<double> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTf2, false);
}

Status Sytf2Rk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<float>> matrix,
               DenseBlasVectorView<std::complex<float>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTf2, false);
}

Status Sytf2Rk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<double>> matrix,
               DenseBlasVectorView<std::complex<double>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QueryHetf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTf2, true);
}

Status Hetf2Rk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<float>> matrix,
               DenseBlasVectorView<std::complex<float>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTf2, true);
}

Result<LapackWorkspacePlan> QueryHetf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTf2, true);
}

Status Hetf2Rk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<double>> matrix,
               DenseBlasVectorView<std::complex<double>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTf2, true);
}

Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTrf, false);
}

Status SytrfRk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
               DenseBlasVectorView<float> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTrf, false);
}

Status SytrfRk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
               DenseBlasVectorView<double> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTrf, false);
}

Status SytrfRk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<float>> matrix,
               DenseBlasVectorView<std::complex<float>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTrf, false);
}

Status SytrfRk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<double>> matrix,
               DenseBlasVectorView<std::complex<double>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QueryHetrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTrf, true);
}

Status HetrfRk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<float>> matrix,
               DenseBlasVectorView<std::complex<float>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTrf, true);
}

Result<LapackWorkspacePlan> QueryHetrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, off_diagonal, pivots,
                     rk::Routine::kTrf, true);
}

Status HetrfRk(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<double>> matrix,
               DenseBlasVectorView<std::complex<double>> off_diagonal,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, off_diagonal, pivots, plan,
                       workspace, report, rk::Routine::kTrf, true);
}

}  // namespace asc
