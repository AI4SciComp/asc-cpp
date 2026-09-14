#include <algorithm>
#include <array>
#include <complex>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_permutation.h"
#include "internal_layout.h"
#include "internal_matrix_permutation_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
using Permutation = DenseBlasVectorView<const index_t>;

template <bool Rows, typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return Rows ? "slapmr" : "slapmt";
  } else if constexpr (std::is_same_v<T, double>) {
    return Rows ? "dlapmr" : "dlapmt";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Rows ? "clapmr" : "clapmt";
  } else {
    return Rows ? "zlapmr" : "zlapmt";
  }
}

template <bool Rows, typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackPermutationDirection direction,
                                  DenseBlasMatrixView<T> matrix,
                                  Permutation permutation) {
  if (direction != LapackPermutationDirection::kForward &&
      direction != LapackPermutationDirection::kBackward) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto m = matrix.rows();
  const auto n = matrix.columns();
  const auto d = Rows ? m : n;
  Status status = checked::Vector(provider, permutation, d);
  if (!status.ok()) {
    return status;
  }
  const std::array spans{ConstMemoryView(matrix.reachable_storage()),
                         permutation.reachable_storage()};
  status = checked::Disjoint(spans);
  if (status.ok()) {
    status = checked::Access(provider, spans[0]);
  }
  if (!status.ok()) {
    return status;
  }
  const auto count =
      internal_matrix_permutation_counts::Count(m, n, checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<Rows, T>(), checked::Kind<T>(),
      std::array{m, n, matrix.leading_dimension(), permutation.size(),
                 permutation.increment()},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(direction),
                                  static_cast<std::int64_t>(matrix.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (*count != 0) {
    plan.regions[checked::kScratch] = {*count, *count, sizeof(T), alignof(T)};
    plan.regions[checked::kInteger] = {d, d, sizeof(lapack_int),
                                       alignof(lapack_int)};
  }
  status = internal_lapack_layout::CheckTotal(plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

Status ConvertPermutation(Permutation permutation, lapack_int* native) {
  const auto d = permutation.size();
  std::fill_n(native, d, lapack_int{0});
  for (extent_t i = 0; i < d; ++i) {
    const auto value = permutation.data()[i];
    if (value < 1 || value > d) {
      return Status(ErrorCode::kIndex);
    }
    if (native[value - 1] != 0) {
      return Status(ErrorCode::kIndex);
    }
    native[value - 1] = 1;
  }
  for (extent_t i = 0; i < d; ++i) {
    native[i] = static_cast<lapack_int>(permutation.data()[i]);
  }
  return Status::Ok();
}

template <typename T>
extent_t Offset(DenseBlasMatrixView<T> matrix, extent_t i, extent_t j) {
  return matrix.layout() == DenseBlasLayout::kColumnMajor
             ? j * matrix.leading_dimension() + i
             : i * matrix.leading_dimension() + j;
}

template <bool Rows, typename T>
void Native(lapack_logical forward, lapack_int m, lapack_int n, T* matrix,
            lapack_int* permutation) {
  if constexpr (std::is_same_v<T, float>) {
    if constexpr (Rows) {
      LAPACK_slapmr(&forward, &m, &n, matrix, &m, permutation);
    } else {
      LAPACK_slapmt(&forward, &m, &n, matrix, &m, permutation);
    }
  } else if constexpr (std::is_same_v<T, double>) {
    if constexpr (Rows) {
      LAPACK_dlapmr(&forward, &m, &n, matrix, &m, permutation);
    } else {
      LAPACK_dlapmt(&forward, &m, &n, matrix, &m, permutation);
    }
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    if constexpr (Rows) {
      LAPACK_clapmr(&forward, &m, &n, matrix, &m, permutation);
    } else {
      LAPACK_clapmt(&forward, &m, &n, matrix, &m, permutation);
    }
  } else {
    if constexpr (Rows) {
      LAPACK_zlapmr(&forward, &m, &n, matrix, &m, permutation);
    } else {
      LAPACK_zlapmt(&forward, &m, &n, matrix, &m, permutation);
    }
  }
}

template <bool Rows, typename T>
Status Execute(const ReferenceLapackProvider& provider,
               LapackPermutationDirection direction,
               DenseBlasMatrixView<T> matrix, Permutation permutation,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array spans{ConstMemoryView(matrix.reachable_storage()),
                         permutation.reachable_storage()};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<Rows, T>(), report);
  const auto expected = Query<Rows>(provider, direction, matrix, permutation);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  const auto m = matrix.rows();
  const auto n = matrix.columns();
  if (m == 0 || n == 0) {
    return checked::Complete(report);
  }
  auto* native_k =
      checked::IntegerObjects(checked::kInteger, permutation.size(), workspace);
  status = ConvertPermutation(permutation, native_k);
  if (!status.ok()) {
    return status;
  }
  auto* staged = static_cast<T*>(workspace.regions[checked::kScratch].data());
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < m; ++i) {
      staged[j * m + i] = matrix.data()[Offset(matrix, i, j)];
    }
  }
  const lapack_logical forward =
      direction == LapackPermutationDirection::kForward ? 1 : 0;
  report.called_provider = true;
  Native<Rows>(forward, static_cast<lapack_int>(m), static_cast<lapack_int>(n),
               staged, native_k);
  for (extent_t i = 0; i < permutation.size(); ++i) {
    if (native_k[i] != permutation.data()[i]) {
      report.outcome = LapackOutcome::kPartialResult;
      report.output_validity = LapackOutputValidity::kUnusable;
      return Status(ErrorCode::kProvider);
    }
  }
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < m; ++i) {
      matrix.data()[Offset(matrix, i, j)] = staged[j * m + i];
    }
  }
  return checked::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<true>(provider, direction, matrix, permutation);
}
Status Lapmr(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true>(provider, direction, matrix, permutation, plan,
                       workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<true>(provider, direction, matrix, permutation);
}
Status Lapmr(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true>(provider, direction, matrix, permutation, plan,
                       workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<true>(provider, direction, matrix, permutation);
}
Status Lapmr(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true>(provider, direction, matrix, permutation, plan,
                       workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmrWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<true>(provider, direction, matrix, permutation);
}
Status Lapmr(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<true>(provider, direction, matrix, permutation, plan,
                       workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<false>(provider, direction, matrix, permutation);
}
Status Lapmt(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<float> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false>(provider, direction, matrix, permutation, plan,
                        workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<false>(provider, direction, matrix, permutation);
}
Status Lapmt(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<double> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false>(provider, direction, matrix, permutation, plan,
                        workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<false>(provider, direction, matrix, permutation);
}
Status Lapmt(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<std::complex<float>> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false>(provider, direction, matrix, permutation, plan,
                        workspace, report);
}
Result<LapackWorkspacePlan> QueryLapmtWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPermutationDirection direction,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const index_t> permutation) {
  return Query<false>(provider, direction, matrix, permutation);
}
Status Lapmt(const ReferenceLapackProvider& provider,
             LapackPermutationDirection direction,
             DenseBlasMatrixView<std::complex<double>> matrix,
             DenseBlasVectorView<const index_t> permutation,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<false>(provider, direction, matrix, permutation, plan,
                        workspace, report);
}
}  // namespace asc
