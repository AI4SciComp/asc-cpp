#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <new>  // IWYU pragma: keep; caller-owned foreign integer lifetimes.
#include <span>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_counts.h"

namespace asc {
namespace {

namespace bk = internal_indefinite;

template <typename T>
Status Symmetry(LapackBunchKaufmanSymmetry symmetry) {
  if (symmetry == LapackBunchKaufmanSymmetry::kSymmetric ||
      (DenseBlasComplex<T> &&
       symmetry == LapackBunchKaufmanSymmetry::kHermitian)) {
    return Status::Ok();
  }
  return Status(ErrorCode::kInvalidArgument);
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
Status Origin(const LapackReport& report, bool hermitian, extent_t order) {
  const auto* const end =
      std::find(report.routine.begin(), report.routine.end(), '\0');
  if (end == report.routine.end()) {
    return Status(ErrorCode::kInvalidState);
  }
  const std::string_view routine(
      report.routine.data(),
      static_cast<std::size_t>(end - report.routine.begin()));
  if (routine != bk::Name<T>(bk::Routine::kTrf, hermitian) &&
      routine != bk::Name<T>(bk::Routine::kTf2, hermitian)) {
    return Status(ErrorCode::kInvalidState);
  }
  if (report.outcome != LapackOutcome::kSuccess ||
      report.output_validity != LapackOutputValidity::kComplete ||
      report.factor_family != LapackFactorFamily::kBunchKaufman ||
      (report.native_info.has_value() && *report.native_info != 0) ||
      (order != 0 &&
       (!report.called_provider || !report.native_info.has_value()))) {
    return Status(ErrorCode::kInvalidState);
  }
  return Status::Ok();
}

template <typename T>
Status FactorMetadata(const ReferenceLapackProvider& provider,
                      DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
                      DenseBlasVectorView<index_t> pivots, bool hermitian) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || pivots.size() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  if (pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array spans{matrix.reachable_storage(), pivots.reachable_storage(),
                         bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, matrix, hermitian),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return internal_indefinite_counts::Factor(
      matrix.rows(), bk::Leading(matrix, hermitian), bk::kIntegerLimit);
}

template <typename T>
Result<LapackWorkspacePlan> QueryFactor(const ReferenceLapackProvider& provider,
                                        DenseBlasTriangle triangle,
                                        DenseBlasMatrixView<T> matrix,
                                        DenseBlasVectorView<index_t> pivots,
                                        bk::Routine routine, bool hermitian) {
  Status metadata =
      FactorMetadata(provider, triangle, matrix, pivots, hermitian);
  if (!metadata.ok()) {
    return metadata;
  }
  const std::array dimensions{matrix.rows(), matrix.columns(),
                              bk::Leading(matrix, hermitian), pivots.size()};
  const std::array<std::int64_t, 4> options{
      static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(hermitian),
      static_cast<std::int64_t>(matrix.layout()), matrix.leading_dimension()};
  const auto identity = LapackPlanIdentity::Create(
      bk::Name<T>(routine, hermitian), bk::ScalarKind<T>(), dimensions, options,
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  if (routine == bk::Routine::kTrf) {
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
                    DenseBlasVectorView<index_t> pivots,
                    DenseBlasTriangle triangle, bool hermitian, const T* packed,
                    const lapack_int* native_pivots, lapack_int info,
                    LapackReport& report) {
  if (info < 0 || info > matrix.rows()) {
    return bk::Defect(info, report);
  }
  const auto native_span = std::span<const lapack_int>(
      native_pivots, static_cast<std::size_t>(matrix.rows()));
  if (!bk::Paired(native_span, matrix.rows(), triangle).ok()) {
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
                     DenseBlasVectorView<index_t> pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report,
                     bk::Routine routine, bool hermitian) {
  const std::array operands{matrix.reachable_storage(),
                            pivots.reachable_storage()};
  Status metadata = bk::Metadata(provider, plan, workspace, report, operands);
  if (!metadata.ok()) {
    return metadata;
  }
  bk::Start(provider, bk::Name<T>(routine, hermitian), report);
  report.factor_family = LapackFactorFamily::kBunchKaufman;
  const auto expected =
      QueryFactor(provider, triangle, matrix, pivots, routine, hermitian);
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
  lapack_int info = 0;
  report.called_provider = true;
  if (routine == bk::Routine::kTrf) {
    auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
    const auto entries = std::min<std::size_t>(
        workspace.regions[bk::kScalar].size() / sizeof(T),
        static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
    bk::TrfCall(hermitian, bk::Uplo(triangle), n, packed, lda, native_pivots,
                work, static_cast<lapack_int>(entries), info);
    if (info >= 0 &&
        work[0] != T{static_cast<DenseBlasRealType<T>>(
                       plan.regions[bk::kScalar].preferred_entries)}) {
      return bk::Defect(info, report);
    }
  } else {
    bk::Tf2Call(hermitian, bk::Uplo(triangle), n, packed, lda, native_pivots,
                info);
  }
  return FinishFactor(matrix, pivots, triangle, hermitian, packed,
                      native_pivots, info, report);
}

template <typename T>
Status SolveMetadata(const ReferenceLapackProvider& provider,
                     const ReferenceBunchKaufmanFactorView<T>& factor,
                     DenseBlasMatrixView<T> rhs, bool hermitian) {
  const auto matrix = factor.factors();
  const auto required = hermitian ? LapackBunchKaufmanSymmetry::kHermitian
                                  : LapackBunchKaufmanSymmetry::kSymmetric;
  if (factor.provider() != provider.identity() ||
      factor.symmetry() != required || !bk::Triangle(factor.triangle())) {
    return Status(ErrorCode::kInvalidState);
  }
  if (factor.originating_routine() !=
          bk::Name<T>(bk::Routine::kTrf, hermitian) &&
      factor.originating_routine() !=
          bk::Name<T>(bk::Routine::kTf2, hermitian)) {
    return Status(ErrorCode::kInvalidState);
  }
  if (matrix.rows() != matrix.columns() || matrix.rows() != rhs.rows()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{
      matrix.reachable_storage(), factor.pivots().reachable_storage(),
      rhs.reachable_storage(), bk::Object(factor), bk::Object(provider)};
  for (const Status& status :
       {bk::Matrix(provider, matrix), bk::Matrix(provider, rhs),
        PivotMetadata(provider, factor.pivots(), matrix.rows()),
        bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return internal_indefinite_counts::Solve(matrix.rows(), rhs.columns(),
                                           bk::Leading(rhs), bk::kIntegerLimit);
}

template <typename T>
Result<LapackWorkspacePlan> QuerySolve(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<T>& factor,
    DenseBlasMatrixView<T> rhs, bool hermitian) {
  Status metadata = SolveMetadata(provider, factor, rhs, hermitian);
  if (!metadata.ok()) {
    return metadata;
  }
  const auto matrix = factor.factors();
  const std::array dimensions{matrix.rows(), rhs.columns(), bk::Leading(matrix),
                              bk::Leading(rhs)};
  const std::array<std::int64_t, 6> options{
      static_cast<std::int64_t>(factor.triangle()),
      static_cast<std::int64_t>(hermitian),
      static_cast<std::int64_t>(matrix.layout()),
      matrix.leading_dimension(),
      static_cast<std::int64_t>(rhs.layout()),
      rhs.leading_dimension()};
  const auto identity = LapackPlanIdentity::Create(
      bk::Name<T>(bk::Routine::kTrs, hermitian), bk::ScalarKind<T>(),
      dimensions, options, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0 || rhs.columns() == 0) {
    return plan;
  }
  plan.regions[bk::kPivot] = {matrix.rows(), matrix.rows(), sizeof(lapack_int),
                              alignof(lapack_int)};
  for (const Status& status :
       {bk::Packing(matrix, plan), bk::Packing(rhs, plan)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

template <typename T>
T Conjugate(T value) {
  if constexpr (DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}

template <typename T>
Status BlockDivisors(const ReferenceBunchKaufmanFactorView<T>& factor,
                     LapackReport& report) {
  const auto matrix = factor.factors();
  const auto pivots = factor.pivots().values();
  const bool hermitian =
      factor.symmetry() == LapackBunchKaufmanSymmetry::kHermitian;
  for (extent_t i = 0; i < matrix.rows();) {
    bool zero = false;
    if (pivots[static_cast<std::size_t>(i)] > 0) {
      zero = hermitian ? bk::Real(bk::Entry(matrix, i, i)) == 0
                       : bk::Entry(matrix, i, i) == T{};
      ++i;
    } else {
      const T offdiagonal = factor.triangle() == DenseBlasTriangle::kUpper
                                ? bk::Entry(matrix, i, i + 1)
                                : bk::Entry(matrix, i + 1, i);
      zero = offdiagonal == T{};
      if (!zero) {
        const T first =
            hermitian && factor.triangle() == DenseBlasTriangle::kLower
                ? Conjugate(offdiagonal)
                : offdiagonal;
        const T second =
            hermitian && factor.triangle() == DenseBlasTriangle::kUpper
                ? Conjugate(offdiagonal)
                : offdiagonal;
        zero = (bk::Entry(matrix, i, i) / first) *
                       (bk::Entry(matrix, i + 1, i + 1) / second) -
                   T{1} ==
               T{};
      }
      i += 2;
    }
    if (zero) {
      report.outcome = LapackOutcome::kSingular;
      report.diagnostic_index =
          i - (pivots[static_cast<std::size_t>(i - 1)] > 0 ? 1 : 2);
      return Status(ErrorCode::kNumerical);
    }
  }
  return Status::Ok();
}

template <typename T>
Status ExecuteSolve(const ReferenceLapackProvider& provider,
                    const ReferenceBunchKaufmanFactorView<T>& factor,
                    DenseBlasMatrixView<T> rhs, const LapackWorkspacePlan& plan,
                    const LapackWorkspace& workspace, LapackReport& report,
                    bool hermitian) {
  const auto matrix = factor.factors();
  const std::array operands{matrix.reachable_storage(),
                            factor.pivots().reachable_storage(),
                            rhs.reachable_storage(), bk::Object(factor)};
  Status metadata = bk::Metadata(provider, plan, workspace, report, operands);
  if (!metadata.ok()) {
    return metadata;
  }
  bk::Start(provider, bk::Name<T>(bk::Routine::kTrs, hermitian), report);
  const auto expected = QuerySolve(provider, factor, rhs, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  Status workspace_valid =
      bk::Plan(provider, *expected, plan, workspace, operands);
  if (!workspace_valid.ok()) {
    return workspace_valid;
  }
  Status pivots_valid =
      bk::Paired(factor.pivots().values(), matrix.rows(), factor.triangle());
  if (!pivots_valid.ok()) {
    return pivots_valid;
  }
  if (matrix.rows() == 0 || rhs.columns() == 0) {
    return bk::Complete(report);
  }
  Status divisors = BlockDivisors(factor, report);
  if (!divisors.ok()) {
    return divisors;
  }
  const auto n = static_cast<lapack_int>(matrix.rows());
  auto* native_pivots = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(n)];
  for (extent_t i = 0; i < matrix.rows(); ++i) {
    native_pivots[i] = static_cast<lapack_int>(
        factor.pivots().values()[static_cast<std::size_t>(i)]);
  }
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const T* packed = bk::PackTriangle(matrix, factor.triangle(), false, cursor);
  T* packed_rhs = bk::PackRhs(rhs, cursor);
  lapack_int info = 0;
  report.called_provider = true;
  bk::TrsCall(hermitian, bk::Uplo(factor.triangle()), n,
              static_cast<lapack_int>(rhs.columns()), packed,
              static_cast<lapack_int>(bk::Leading(matrix)), native_pivots,
              packed_rhs, static_cast<lapack_int>(bk::Leading(rhs)), info);
  report.native_info = info;
  if (info != 0) {
    return bk::Defect(info, report);
  }
  bk::PublishRhs(packed_rhs, rhs);
  return bk::Complete(report);
}

}  // namespace

template <DenseBlasScalar Element>
Result<ReferenceBunchKaufmanFactorView<Element>>
ReferenceBunchKaufmanFactorView<Element>::Create(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const Element> factors, DenseBlasTriangle triangle,
    LapackBunchKaufmanSymmetry symmetry, RawLapackPivotView pivots,
    const LapackReport& report) {
  if (!bk::Triangle(triangle) || !Symmetry<Element>(symmetry).ok()) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (factors.rows() != factors.columns()) {
    return Status(ErrorCode::kShape);
  }
  if (report.provider != provider.identity()) {
    return Status(ErrorCode::kInvalidState);
  }
  const bool hermitian = symmetry == LapackBunchKaufmanSymmetry::kHermitian;
  const std::array spans{factors.reachable_storage(),
                         pivots.reachable_storage(), bk::Object(provider),
                         bk::Object(report)};
  for (const Status& status :
       {bk::Matrix(provider, factors),
        PivotMetadata(provider, pivots, factors.rows()),
        Origin<Element>(report, hermitian, factors.rows()),
        bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  Status encoded = bk::Paired(pivots.values(), factors.rows(), triangle);
  if (!encoded.ok()) {
    return encoded;
  }
  return ReferenceBunchKaufmanFactorView(factors, pivots, triangle, symmetry,
                                         report);
}

template class ReferenceBunchKaufmanFactorView<float>;
template class ReferenceBunchKaufmanFactorView<double>;
template class ReferenceBunchKaufmanFactorView<std::complex<float>>;
template class ReferenceBunchKaufmanFactorView<std::complex<double>>;

Result<LapackWorkspacePlan> QuerySytrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTrf,
                     false);
}

Status Sytrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTrf,
                     false);
}

Status Sytrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTrf,
                     false);
}

Status Sytrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTrf,
                     false);
}

Status Sytrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTrf, false);
}

Result<LapackWorkspacePlan> QuerySytf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTf2,
                     false);
}

Status Sytf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QuerySytf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTf2,
                     false);
}

Status Sytf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QuerySytf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTf2,
                     false);
}

Status Sytf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QuerySytf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTf2,
                     false);
}

Status Sytf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTf2, false);
}

Result<LapackWorkspacePlan> QueryHetrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTrf,
                     true);
}

Status Hetrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTrf, true);
}

Result<LapackWorkspacePlan> QueryHetrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTrf,
                     true);
}

Status Hetrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTrf, true);
}

Result<LapackWorkspacePlan> QueryHetf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTf2,
                     true);
}

Status Hetf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTf2, true);
}

Result<LapackWorkspacePlan> QueryHetf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return QueryFactor(provider, triangle, matrix, pivots, bk::Routine::kTf2,
                     true);
}

Status Hetf2(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteFactor(provider, triangle, matrix, pivots, plan, workspace,
                       report, bk::Routine::kTf2, true);
}

Result<LapackWorkspacePlan> QuerySytrsWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<float>& factor,
    DenseBlasMatrixView<float> rhs) {
  return QuerySolve(provider, factor, rhs, false);
}

Status Sytrs(const ReferenceLapackProvider& provider,
             const ReferenceBunchKaufmanFactorView<float>& factor,
             DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySytrsWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<double>& factor,
    DenseBlasMatrixView<double> rhs) {
  return QuerySolve(provider, factor, rhs, false);
}

Status Sytrs(const ReferenceLapackProvider& provider,
             const ReferenceBunchKaufmanFactorView<double>& factor,
             DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySytrsWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<std::complex<float>>& factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, factor, rhs, false);
}

Status Sytrs(const ReferenceLapackProvider& provider,
             const ReferenceBunchKaufmanFactorView<std::complex<float>>& factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QuerySytrsWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<std::complex<double>>& factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, factor, rhs, false);
}

Status Sytrs(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<std::complex<double>>& factor,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report, false);
}

Result<LapackWorkspacePlan> QueryHetrsWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<std::complex<float>>& factor,
    DenseBlasMatrixView<std::complex<float>> rhs) {
  return QuerySolve(provider, factor, rhs, true);
}

Status Hetrs(const ReferenceLapackProvider& provider,
             const ReferenceBunchKaufmanFactorView<std::complex<float>>& factor,
             DenseBlasMatrixView<std::complex<float>> rhs,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report, true);
}

Result<LapackWorkspacePlan> QueryHetrsWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<std::complex<double>>& factor,
    DenseBlasMatrixView<std::complex<double>> rhs) {
  return QuerySolve(provider, factor, rhs, true);
}

Status Hetrs(
    const ReferenceLapackProvider& provider,
    const ReferenceBunchKaufmanFactorView<std::complex<double>>& factor,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report) {
  return ExecuteSolve(provider, factor, rhs, plan, workspace, report, true);
}

}  // namespace asc
