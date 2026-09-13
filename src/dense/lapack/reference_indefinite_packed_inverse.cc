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
#include "asc/dense/providers/lapack_indefinite_packed_inverse.h"
#include "internal_indefinite.h"
#include "internal_indefinite_calls.h"
#include "internal_indefinite_expert.h"
#include "internal_indefinite_packed_inverse_counts.h"
#include "internal_packed_triangular.h"

namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace packed = internal_packed_triangular;

template <typename T>
std::string_view Name(bool hermitian) {
  if constexpr (std::is_same_v<T, float>) {
    return "SSPTRI";
  } else if constexpr (std::is_same_v<T, double>) {
    return "DSPTRI";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return hermitian ? "CHPTRI" : "CSPTRI";
  } else {
    return hermitian ? "ZHPTRI" : "ZSPTRI";
  }
}

template <typename T>
void Call(bool hermitian, char triangle, lapack_int n, T* a,
          const lapack_int* pivots, T* work, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssptri(&triangle, &n, a, pivots, work, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsptri(&triangle, &n, a, pivots, work, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (hermitian) {
      LAPACK_chptri(&triangle, &n, a, pivots, work, &info);
    } else {
      LAPACK_csptri(&triangle, &n, a, pivots, work, &info);
    }
  } else {
    if (hermitian) {
      LAPACK_zhptri(&triangle, &n, a, pivots, work, &info);
    } else {
      LAPACK_zsptri(&triangle, &n, a, pivots, work, &info);
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
                                  DenseBlasPackedMatrixView<T> factors,
                                  RawLapackPivotView pivots, bool hermitian) {
  if (!bk::Triangle(triangle)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const std::array spans{factors.reachable_storage(),
                         pivots.reachable_storage(), bk::Object(provider)};
  for (const Status& status :
       {bk::Accessible(provider, factors.reachable_storage()),
        PivotMetadata(provider, pivots, factors.order()), bk::Disjoint(spans),
        internal_indefinite_packed_inverse_counts::Inverse(
            factors.order(), bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(hermitian), bk::ScalarKind<T>(), std::array{factors.order()},
      std::array<std::int64_t, 4>{
          static_cast<std::int64_t>(triangle),
          static_cast<std::int64_t>(hermitian),
          static_cast<std::int64_t>(factors.layout()),
          static_cast<std::int64_t>(pivots.values().size())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (factors.order() != 0) {
    const auto n = factors.order();
    // All six packed routines document N same-scalar WORK entries.
    plan.regions[bk::kScalar] = {n, n, sizeof(T), alignof(T)};
    plan.regions[bk::kPivot] = {n, n, sizeof(lapack_int), alignof(lapack_int)};
    const Status status = packed::Packing(factors, plan);
    if (!status.ok()) {
      return status;
    }
  }
  return plan;
}

// Mirror only the source's immutable early singular scan. Do not replace
// native inversion or infer a 2x2/finiteness diagnosis absent upstream.
template <typename T>
lapack_int SingularInfo(DenseBlasPackedMatrixView<T> factors,
                        RawLapackPivotView pivots, DenseBlasTriangle triangle) {
  for (extent_t offset = 0; offset < factors.order(); ++offset) {
    const extent_t i = triangle == DenseBlasTriangle::kUpper
                           ? factors.order() - 1 - offset
                           : offset;
    if (pivots.values()[static_cast<std::size_t>(i)] > 0 &&
        factors.data()[packed::Offset(factors.order(), triangle,
                                      factors.layout(), i, i)] == T{}) {
      return static_cast<lapack_int>(i + 1);
    }
  }
  return 0;
}

template <typename T>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasPackedMatrixView<T> factors,
               RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool hermitian) {
  const std::array operands{factors.reachable_storage(),
                            pivots.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(hermitian), report);
  const auto expected = Query(provider, triangle, factors, pivots, hermitian);
  if (!expected.ok()) {
    return expected.status();
  }
  status = bk::Plan(provider, *expected, plan, workspace, operands);
  if (!status.ok()) {
    return status;
  }
  if (factors.order() == 0) {
    return bk::Complete(report);
  }
  status = bk::Paired(pivots.values(), factors.order(), triangle);
  if (!status.ok()) {
    return status;
  }
  const lapack_int expected_info = SingularInfo(factors, pivots, triangle);
  auto* native_pivots =
      internal_indefinite_expert::PreparePivots(pivots, plan, workspace);
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* native_factors =
      packed::Pack(factors, triangle, DenseBlasDiagonal::kNonUnit, cursor);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(hermitian, bk::Uplo(triangle), static_cast<lapack_int>(factors.order()),
       native_factors, native_pivots, work, info);
  report.native_info = info;
  for (std::size_t i = 0; i < pivots.values().size(); ++i) {
    if (native_pivots[i] != pivots.values()[i]) {
      return bk::Defect(info, report);
    }
  }
  if (info != expected_info) {
    return bk::Defect(info, report);
  }
  if (info > 0) {
    report.outcome = LapackOutcome::kSingular;
    report.output_validity = LapackOutputValidity::kDocumentedPartial;
    report.diagnostic_index = info - 1;
    return Status(ErrorCode::kNumerical);
  }
  packed::Publish(native_factors, factors, triangle,
                  DenseBlasDiagonal::kNonUnit);
  return bk::Complete(report);
}

}  // namespace

Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> factors, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots, false);
}
Status Sptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<float> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 false);
}

Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> factors, RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots, false);
}
Status Sptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<double> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 false);
}

Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots, false);
}
Status Sptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<float>> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 false);
}

Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots, false);
}
Status Sptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<double>> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 false);
}

Result<LapackWorkspacePlan> QueryHptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots, true);
}
Status Hptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<float>> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 true);
}

Result<LapackWorkspacePlan> QueryHptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots) {
  return Query(provider, triangle, factors, pivots, true);
}
Status Hptri(const ReferenceLapackProvider& provider,
             DenseBlasTriangle triangle,
             DenseBlasPackedMatrixView<std::complex<double>> factors,
             RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, factors, pivots, plan, workspace, report,
                 true);
}

}  // namespace asc
