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
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage.h"
#include "internal_indefinite.h"
#include "internal_indefinite_aasen_two_stage_counts.h"
#include "internal_indefinite_calls.h"
namespace asc {
namespace {
namespace bk = internal_indefinite;
namespace counts = internal_indefinite_aasen_two_stage_counts;
template <typename T>
std::string_view Name(bool he) {
  if constexpr (std::is_same_v<T, float>) {
    return "ssytrf_aa_2stage";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dsytrf_aa_2stage";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return he ? "chetrf_aa_2stage" : "csytrf_aa_2stage";
  } else {
    return he ? "zhetrf_aa_2stage" : "zsytrf_aa_2stage";
  }
}
template <typename T>
void Call(bool he, char triangle, lapack_int n, T* a, lapack_int lda, T* tb,
          lapack_int ltb, lapack_int* pivots, lapack_int* band_pivots, T* work,
          lapack_int lwork, lapack_int& info) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_ssytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                            band_pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dsytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                            band_pivots, work, &lwork, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if (he) {
      LAPACK_chetrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    } else {
      LAPACK_csytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    }
  } else if constexpr (std::is_same_v<T, std::complex<double>>) {
    if (he) {
      LAPACK_zhetrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    } else {
      LAPACK_zsytrf_aa_2stage(&triangle, &n, a, &lda, tb, &ltb, pivots,
                              band_pivots, work, &lwork, &info);
    }
  }
}

template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<T> matrix,
                                  DenseBlasVectorView<T> band,
                                  DenseBlasVectorView<index_t> pivots,
                                  DenseBlasVectorView<index_t> band_pivots,
                                  bool he) {
  if (!bk::Triangle(triangle) || band.increment() != 1 ||
      pivots.increment() != 1 || band_pivots.increment() != 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (matrix.rows() != matrix.columns() || matrix.rows() != pivots.size() ||
      matrix.rows() != band_pivots.size()) {
    return Status(ErrorCode::kShape);
  }
  if (matrix.leading_dimension() > bk::kIntegerLimit) {
    return Status(ErrorCode::kOverflow);
  }
  const std::array spans{matrix.reachable_storage(), band.reachable_storage(),
                         pivots.reachable_storage(),
                         band_pivots.reachable_storage(), bk::Object(provider)};
  for (const auto& status :
       {bk::Matrix(provider, matrix, he),
        bk::Accessible(provider, band.reachable_storage()),
        bk::Accessible(provider, pivots.reachable_storage()),
        bk::Accessible(provider, band_pivots.reachable_storage()),
        bk::Disjoint(spans),
        counts::Factor(matrix.rows(), bk::Leading(matrix, he), band.size(),
                       bk::kIntegerLimit)}) {
    if (!status.ok()) {
      return status;
    }
  }
  const auto identity = LapackPlanIdentity::Create(
      Name<T>(he), bk::ScalarKind<T>(),
      std::array{matrix.rows(), bk::Leading(matrix, he), band.size(),
                 pivots.size(), band_pivots.size()},
      std::array<std::int64_t, 7>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(he),
                                  static_cast<std::int64_t>(matrix.layout()),
                                  matrix.leading_dimension(), band.increment(),
                                  pivots.increment(), band_pivots.increment()},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0) {
    return plan;
  }
  const auto preferred =
      counts::Preferred<DenseBlasRealType<T>>(matrix.rows(), bk::kIntegerLimit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  const auto n = matrix.rows();
  plan.regions[bk::kScalar] = {n, *preferred, sizeof(T), alignof(T)};
  plan.regions[bk::kPivot] = {2 * n, 2 * n, sizeof(lapack_int),
                              alignof(lapack_int)};
  const auto packed = bk::Packing(matrix, plan, he);
  if (!packed.ok()) {
    return packed;
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
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle, DenseBlasMatrixView<T> matrix,
               DenseBlasVectorView<T> band, DenseBlasVectorView<index_t> pivots,
               DenseBlasVectorView<index_t> band_pivots,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report,
               bool he) {
  const std::array operands{
      matrix.reachable_storage(), band.reachable_storage(),
      pivots.reachable_storage(), band_pivots.reachable_storage()};
  Status status = bk::Metadata(provider, plan, workspace, report, operands);
  if (!status.ok()) {
    return status;
  }
  bk::Start(provider, Name<T>(he), report);
  report.factor_family = LapackFactorFamily::kAasen;
  const auto expected =
      Query(provider, triangle, matrix, band, pivots, band_pivots, he);
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
  const auto nb =
      static_cast<lapack_int>(counts::BlockWidth(n, band.size(), entries));
  const auto ldtb = band.size() / n;
  auto* native = ::new (workspace.regions[bk::kPivot].data())
      lapack_int[static_cast<std::size_t>(2 * n)];
  std::fill_n(native, 2 * n, std::numeric_limits<lapack_int>::min());
  auto* cursor = static_cast<T*>(workspace.regions[bk::kLayout].data());
  T* packed = bk::PackTriangle(matrix, triangle, he, cursor, he);
  T sentinel{-1};
  if constexpr (DenseBlasComplex<T>) {
    sentinel.imag(-1);
  }
  band.data()[0] = sentinel;
  for (lapack_int j = 0; j < n; ++j) {
    band.data()[j * ldtb + 2 * nb] = sentinel;
  }
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Call(he, bk::Uplo(triangle), n, packed,
       static_cast<lapack_int>(bk::Leading(matrix, he)), band.data(),
       static_cast<lapack_int>(band.size()), native, native + n,
       static_cast<T*>(workspace.regions[bk::kScalar].data()),
       static_cast<lapack_int>(entries), info);
  report.native_info = info;
  if (info < 0 || info > n ||
      band.data()[0] != T{static_cast<DenseBlasRealType<T>>(nb)} ||
      !ValidPivots(native, native + n, n, nb) ||
      (info > 0 && band.data()[(info - 1) * ldtb + 2 * nb] != T{})) {
    return bk::Defect(info, report);
  }
  std::copy_n(native, n, pivots.data());
  std::copy_n(native + n, n, band_pivots.data());
  bk::PublishTriangle(packed, matrix, triangle, he);
  if (info == 0) {
    return bk::Complete(report);
  }
  report.diagnostic_index = info - 1;
  report.outcome = LapackOutcome::kSingular;
  report.output_validity = LapackOutputValidity::kDocumentedPartial;
  return Status(ErrorCode::kNumerical);
}
}  // namespace
Result<LapackWorkspacePlan> QuerySytrfAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, false);
}
Status SytrfAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<float> matrix,
                     DenseBlasVectorView<float> band,
                     DenseBlasVectorView<index_t> pivots,
                     DenseBlasVectorView<index_t> band_pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrfAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<double> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, false);
}
Status SytrfAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<double> matrix,
                     DenseBlasVectorView<double> band,
                     DenseBlasVectorView<index_t> pivots,
                     DenseBlasVectorView<index_t> band_pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrfAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, false);
}
Status SytrfAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<std::complex<float>> matrix,
                     DenseBlasVectorView<std::complex<float>> band,
                     DenseBlasVectorView<index_t> pivots,
                     DenseBlasVectorView<index_t> band_pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QuerySytrfAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, false);
}
Status SytrfAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<std::complex<double>> matrix,
                     DenseBlasVectorView<std::complex<double>> band,
                     DenseBlasVectorView<index_t> pivots,
                     DenseBlasVectorView<index_t> band_pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, plan,
                 workspace, report, false);
}
Result<LapackWorkspacePlan> QueryHetrfAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, true);
}
Status HetrfAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<std::complex<float>> matrix,
                     DenseBlasVectorView<std::complex<float>> band,
                     DenseBlasVectorView<index_t> pivots,
                     DenseBlasVectorView<index_t> band_pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, plan,
                 workspace, report, true);
}
Result<LapackWorkspacePlan> QueryHetrfAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots) {
  return Query(provider, triangle, matrix, band, pivots, band_pivots, true);
}
Status HetrfAa2Stage(const ReferenceLapackProvider& provider,
                     DenseBlasTriangle triangle,
                     DenseBlasMatrixView<std::complex<double>> matrix,
                     DenseBlasVectorView<std::complex<double>> band,
                     DenseBlasVectorView<index_t> pivots,
                     DenseBlasVectorView<index_t> band_pivots,
                     const LapackWorkspacePlan& plan,
                     const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, triangle, matrix, band, pivots, band_pivots, plan,
                 workspace, report, true);
}
}  // namespace asc
