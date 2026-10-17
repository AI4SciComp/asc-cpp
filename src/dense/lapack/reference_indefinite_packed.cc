#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; caller-owned native INTEGER lifetimes.
#include <span>
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
#include "asc/dense/providers/lapack_indefinite_packed.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_packed_counts.h"
#include "internal_packed_triangular.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace packed = internal_packed_triangular;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "SSPTRF";
  } else if constexpr (std::is_same_v<T, double>) {
    return "DSPTRF";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "CHPTRF" : "CSPTRF";
  } else {
    return he ? "ZHPTRF" : "ZSPTRF";
  }
}
template <typename T>
Status Metadata(const ReferenceLapackProvider& provider,
                DenseBlasTriangle triangle, DenseBlasPackedMatrixView<T> matrix,
                DenseBlasVectorView<index_t> pivots) {
  if (!bk::Triangle(triangle) || pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (pivots.size() != matrix.order()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{matrix.reachable_storage(), pivots.reachable_storage(),
                         bk::Object(provider)};
  for (const Status& status :
       {bk::Accessible(provider, matrix.reachable_storage()),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Disjoint(spans)}) {
    if (!status.ok()) {
      return status;
    }
  }
  return internal_indefinite_packed_counts::Factor(
      matrix.order(), triangle == DenseBlasTriangle::kUpper, bk::kIntegerLimit);
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasPackedMatrixView<T> matrix,
                                  DenseBlasVectorView<index_t> pivots,
                                  bool he) {
  Status metadata = Metadata(provider, triangle, matrix, pivots);
  if (!metadata.ok()) {
    return metadata;
  }
  const std::array dimensions{matrix.order(), pivots.size()};
  const std::array<std::int64_t, 3> options{
      static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(he),
      static_cast<std::int64_t>(matrix.layout())};
  const auto identity =
      LapackPlanIdentity::Create(Name<T>(he), bk::ScalarKind<T>(), dimensions,
                                 options, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.order() != 0) {
    // The pinned terminal 2x2 step can touch either logical endpoint after
    // reciprocal overflow. Both layouts therefore need guarded native arrays.
    const extent_t packed_count = packed::Entries(matrix.order()) + 2;
    const extent_t pivot_count = matrix.order() + 2;
    plan.regions[bk::kPivot] = {pivot_count, pivot_count, sizeof(lapack_int),
                                alignof(lapack_int)};
    plan.regions[bk::kLayout] = {packed_count, packed_count, sizeof(T),
                                 alignof(T)};
  }
  Status total = bk::Total(plan);
  if (!total.ok()) {
    return total;
  }
  return plan;
}
template <typename T>
void Native(bool he, char uplo, lapack_int n, T* a, lapack_int* pivots,
            lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptrf(&uplo, &n, a, pivots, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptrf(&uplo, &n, a, pivots, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chptrf(&uplo, &n, a, pivots, &info);
    } else {
      LAPACK_csptrf(&uplo, &n, a, pivots, &info);
    }
  } else {
    if (he) {
      LAPACK_zhptrf(&uplo, &n, a, pivots, &info);
    } else {
      LAPACK_zsptrf(&uplo, &n, a, pivots, &info);
    }
  }
}
template <typename T>
T Guard() {
  if constexpr (DenseBlasComplex<T>) {
    return T{-173, 19};
  } else {
    return T{-173};
  }
}
template <typename T>
bool UnsafeScalar(DenseBlasPackedMatrixView<T> matrix, bool he) {
  if (matrix.order() != 1) {
    return false;
  }
  if constexpr (DenseBlasComplex<T>) {
    return std::isnan(matrix.data()[0].real()) ||
           (!he && std::isnan(matrix.data()[0].imag()));
  } else {
    return std::isnan(matrix.data()[0]);
  }
}
template <typename T>
void CopyPacked(DenseBlasPackedMatrixView<T> matrix, DenseBlasTriangle triangle,
                T* a, bool publish) {
  if (matrix.layout() == DenseBlasLayout::kColumnMajor) {
    const auto count = packed::Entries(matrix.order());
    if (publish) {
      std::copy_n(a, count, matrix.data());
    } else {
      std::copy_n(matrix.data(), count, a);
    }
    return;
  }
  for (extent_t j = 0; j < matrix.order(); ++j) {
    const extent_t first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const extent_t last =
        triangle == DenseBlasTriangle::kUpper ? j + 1 : matrix.order();
    for (extent_t i = first; i < last; ++i) {
      const auto source =
          packed::Offset(matrix.order(), triangle, matrix.layout(), i, j);
      const auto column = packed::Offset(matrix.order(), triangle,
                                         DenseBlasLayout::kColumnMajor, i, j);
      if (publish) {
        matrix.data()[source] = a[column];
      } else {
        a[column] = matrix.data()[source];
      }
    }
  }
}
template <typename T>
Status Finish(DenseBlasPackedMatrixView<T> matrix, DenseBlasTriangle triangle,
              DenseBlasVectorView<index_t> pivots, T* a,
              const lapack_int* native_pivots, lapack_int info,
              LapackReport& report) {
  const auto n = matrix.order();
  const auto count = packed::Entries(n);
  constexpr lapack_int kSentinel = std::numeric_limits<lapack_int>::min();
  if (info < 0 || info > n || a[-1] != Guard<T>() || a[count] != Guard<T>() ||
      native_pivots[-1] != kSentinel || native_pivots[n] != kSentinel ||
      !bk::Paired(std::span<const lapack_int>(native_pivots,
                                              static_cast<std::size_t>(n)),
                  n, triangle)
           .ok()) {
    return bk::Defect(info, report);
  }
  for (extent_t i = 0; i < n; ++i) {
    pivots.data()[i] = native_pivots[i];
  }
  CopyPacked(matrix, triangle, a, true);
  report.native_info = info;
  if (info == 0) {
    return bk::Complete(report);
  }
  report.diagnostic_index = static_cast<index_t>(info) - 1;
  const extent_t diagonal =
      packed::Offset(n, triangle, matrix.layout(), info - 1, info - 1);
  report.outcome = matrix.data()[diagonal] == T{}
                       ? LapackOutcome::kSingular
                       : LapackOutcome::kPartialResult;
  report.output_validity = LapackOutputValidity::kDocumentedPartial;
  return Status(ErrorCode::kNumerical);
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasPackedMatrixView<T> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool he) {
  const std::array operands{matrix.reachable_storage(),
                            pivots.reachable_storage()};
  Status metadata = bk::Metadata(provider, plan, workspace, report, operands);
  if (!metadata.ok()) {
    return metadata;
  }
  bk::Start(provider, Name<T>(he), report);
  report.factor_family = LapackFactorFamily::kBunchKaufman;
  const auto expected = Query(provider, triangle, matrix, pivots, he);
  if (!expected.ok()) {
    return expected.status();
  }
  Status valid = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!valid.ok()) {
    return valid;
  }
  if (matrix.order() == 0) {
    return bk::Complete(report);
  }
  // A significant scalar NaN can enter the pinned terminal 2x2 branch
  // without ever assigning IMAX. Return explicit unusable numerical output;
  // do not touch caller scratch or substitute a different factorization.
  if (UnsafeScalar(matrix, he)) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    report.diagnostic_index = 0;
    return Status(ErrorCode::kNumerical);
  }
  const auto n = static_cast<lapack_int>(matrix.order());
  auto* pivot_storage = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(n) + 2];
  std::fill_n(pivot_storage, static_cast<std::size_t>(n) + 2,
              std::numeric_limits<lapack_int>::min());
  auto* native_pivots = pivot_storage + 1;
  auto* storage = static_cast<T*>(workspace.regions[bk::kLayout].data());
  const auto count = packed::Entries(matrix.order());
  storage[0] = Guard<T>();
  storage[count + 1] = Guard<T>();
  auto* a = storage + 1;
  CopyPacked(matrix, triangle, a, false);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native(he, bk::Uplo(triangle), n, a, native_pivots, info);
  return Finish(matrix, triangle, pivots, a, native_pivots, info, report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status Sptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<float> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status Sptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<double> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status Sptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status Sptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QueryHptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, true);
}
Status Hptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 true);
}
Result<LapackWorkspacePlan> QueryHptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, true);
}
Status Hptrf(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<index_t> pivots,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 true);
}
}  // namespace asc
