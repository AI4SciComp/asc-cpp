#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>  // IWYU pragma: keep; explicit caller native INTEGER lifetimes.
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
#include "asc/dense/providers/lapack_indefinite_aasen.h"
#include "internal_indefinite.h"
#include "internal_indefinite_aasen_counts.h"
#include "internal_indefinite_calls.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace counts = internal_indefinite_aasen_counts;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssytrf_aa";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsytrf_aa";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "chetrf_aa" : "csytrf_aa";
  } else {
    return he ? "zhetrf_aa" : "zsytrf_aa";
  }
}
void Call([[maybe_unused]] bool he, char uplo, lapack_int n, float* a,
          lapack_int lda, lapack_int* pivots, float* work, lapack_int lwork,
          lapack_int& info) {
  LAPACK_ssytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
}
void Call([[maybe_unused]] bool he, char uplo, lapack_int n, double* a,
          lapack_int lda, lapack_int* pivots, double* work, lapack_int lwork,
          lapack_int& info) {
  LAPACK_dsytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
}
void Call([[maybe_unused]] bool he, char uplo, lapack_int n,
          std::complex<float>* a, lapack_int lda, lapack_int* pivots,
          std::complex<float>* work, lapack_int lwork, lapack_int& info) {
  if (he) {
    LAPACK_chetrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
  } else {
    LAPACK_csytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
  }
}
void Call([[maybe_unused]] bool he, char uplo, lapack_int n,
          std::complex<double>* a, lapack_int lda, lapack_int* pivots,
          std::complex<double>* work, lapack_int lwork, lapack_int& info) {
  if (he) {
    LAPACK_zhetrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
  } else {
    LAPACK_zsytrf_aa(&uplo, &n, a, &lda, pivots, work, &lwork, &info);
  }
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<T> matrix,
                                  DenseBlasVectorView<index_t> pivots,
                                  bool he) {
  if (!bk::Triangle(triangle) || pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || pivots.size() != matrix.rows()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{matrix.reachable_storage(), pivots.reachable_storage(),
                         bk::Object(provider)};
  for (const auto& status :
       {bk::Matrix(provider, matrix, he),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Disjoint(spans),
        counts::Factor(matrix.rows(), bk::Leading(matrix, he),
                       bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (matrix.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array dimensions{matrix.rows(), matrix.columns(),
                              bk::Leading(matrix, he), pivots.size()};
  const std::array<std::int64_t, 5> options{
      static_cast<std::int64_t>(triangle), static_cast<std::int64_t>(he),
      static_cast<std::int64_t>(matrix.layout()), matrix.leading_dimension(),
      pivots.increment()};
  const auto identity =
      LapackPlanIdentity::Create(Name<T>(he), bk::ScalarKind<T>(), dimensions,
                                 options, provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  const bool complex_symmetric = DenseBlasComplex<T> && !he;
  const auto preferred = counts::Preferred<DenseBlasRealType<T>>(
      matrix.rows(), complex_symmetric, bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  const extent_t minimum =
      matrix.rows() == 1 && !complex_symmetric ? 1 : 2 * matrix.rows();
  plan.regions[bk::kScalar] = {minimum, *preferred, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {matrix.rows(), matrix.rows(), sizeof(lapack_int),
                              alignof(lapack_int)};
  const Status packing = bk::Packing(matrix, plan, he);
  if (!packing.ok()) {
    return packing;
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
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
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
  report.factor_family = LapackFactorFamily::kAasen;
  const auto expected = Query(provider, triangle, matrix, pivots, he);
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
  auto* native_pivots = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(n)];
  std::fill_n(native_pivots, n, std::numeric_limits<lapack_int>::min());
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(matrix, triangle, he, cursor, he);
  auto* work = static_cast<T*>(workspace.regions[bk::kScalar].data());
  work[0] = T{-1};
  if constexpr (DenseBlasComplex<T>) {
    work[0].imag(-1);
  }
  const auto entries = std::min<std::size_t>(
      workspace.regions[bk::kScalar].size() / sizeof(T),
      static_cast<std::size_t>(plan.regions[bk::kScalar].preferred_entries));
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(he, bk::Uplo(triangle), n, packed,
       static_cast<lapack_int>(bk::Leading(matrix, he)), native_pivots, work,
       static_cast<lapack_int>(entries), info);
  if (info != 0 ||
      work[0] != T{static_cast<DenseBlasRealType<T>>(
                     plan.regions[bk::kScalar].preferred_entries)} ||
      !ValidPivots(native_pivots, n)) {
    return bk::Defect(info, report);
  }
  for (lapack_int i = 0; i < n; ++i) {
    pivots.data()[i] = native_pivots[i];
  }
  bk::PublishTriangle(packed, matrix, triangle, he);
  report.native_info = info;
  return bk::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status SytrfAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<float> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status SytrfAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<double> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status SytrfAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<float>> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QuerySytrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, false);
}
Status SytrfAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<double>> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 false);
}
Result<LapackWorkspacePlan> QueryHetrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, true);
}
Status HetrfAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<float>> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 true);
}
Result<LapackWorkspacePlan> QueryHetrfAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots) {
  return Query(provider, triangle, matrix, pivots, true);
}
Status HetrfAa(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<std::complex<double>> matrix,
               DenseBlasVectorView<index_t> pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, pivots, plan, workspace, report,
                 true);
}
}  // namespace asc
