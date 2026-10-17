#include <array>
#include <complex>
#include <cstdint>
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
#include "asc/dense/providers/lapack_matrix_support.h"
#include "internal_layout.h"
#include "internal_matrix_copy_counts.h"
#include "internal_matrix_support_prototypes.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace layout = internal_lapack_layout;
template <typename T, bool Rows>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return Rows ? "ilaslr" : "ilaslc";
  } else if constexpr (std::is_same_v<T, double>) {
    return Rows ? "iladlr" : "iladlc";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Rows ? "ilaclr" : "ilaclc";
  } else {
    return Rows ? "ilazlr" : "ilazlc";
  }
}
template <typename T, bool Rows>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasMatrixView<const T> matrix) {
  Status status = checked::Access(provider, matrix.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<T, Rows>(), checked::Kind<T>(),
      std::array{matrix.rows(), matrix.columns(), matrix.leading_dimension()},
      std::array<std::int64_t, 1>{static_cast<std::int64_t>(matrix.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    return plan;
  }
  // The shared count bounds the same M+1/N+1 loop terminals as these scans.
  const auto count = internal_matrix_copy_counts::Count(
      matrix.rows(), matrix.columns(), checked::Leading(matrix),
      checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  status = layout::AddPacking(matrix, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}
template <typename T, bool Rows>
lapack_int Native(lapack_int& m, lapack_int& n, const T* input,
                  lapack_int& leading) {
  // Exact emitted declarations are mutable; the source reads this operand only.
  auto* matrix = const_cast<T*>(input);
  if constexpr (std::is_same_v<T, float>) {
    return Rows ? ASC_LAPACK_ilaslr(&m, &n, matrix, &leading)
                : ASC_LAPACK_ilaslc(&m, &n, matrix, &leading);
  } else if constexpr (std::is_same_v<T, double>) {
    return Rows ? ASC_LAPACK_iladlr(&m, &n, matrix, &leading)
                : ASC_LAPACK_iladlc(&m, &n, matrix, &leading);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return Rows ? ASC_LAPACK_ilaclr(&m, &n, matrix, &leading)
                : ASC_LAPACK_ilaclc(&m, &n, matrix, &leading);
  } else {
    return Rows ? ASC_LAPACK_ilazlr(&m, &n, matrix, &leading)
                : ASC_LAPACK_ilazlc(&m, &n, matrix, &leading);
  }
}
template <typename T, bool Rows>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasMatrixView<const T> matrix, index_t& extent,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array spans{matrix.reachable_storage(),
                         checked::ObjectStorage(extent)};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<T, Rows>(), report);
  status = checked::Disjoint(spans);
  if (!status.ok()) {
    return status;
  }
  const auto expected = Query<T, Rows>(provider, matrix);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  if (matrix.rows() == 0 || matrix.columns() == 0) {
    extent = 0;
    return checked::Complete(report);
  }
  auto* cursor = static_cast<T*>(workspace.regions[checked::kLayout].data());
  const T* input = layout::Pack(matrix, cursor);
  lapack_int m = static_cast<lapack_int>(matrix.rows());
  lapack_int n = static_cast<lapack_int>(matrix.columns());
  lapack_int leading = static_cast<lapack_int>(checked::Leading(matrix));
  const auto original_m = m;
  const auto original_n = n;
  const auto original_leading = leading;
  report.called_provider = true;
  const lapack_int result = Native<T, Rows>(m, n, input, leading);
  if (m != original_m || n != original_n || leading != original_leading ||
      result < 0 || result > (Rows ? original_m : original_n)) {
    report.outcome = LapackOutcome::kPartialResult;
    report.output_validity = LapackOutputValidity::kUnusable;
    return Status(ErrorCode::kProvider);
  }
  extent = static_cast<index_t>(result);
  return checked::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix) {
  return Query<float, false>(provider, matrix);
}
Status Ilalc(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const float> matrix, index_t& extent,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<float, false>(provider, matrix, extent, plan, workspace,
                               report);
}

Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix) {
  return Query<double, false>(provider, matrix);
}
Status Ilalc(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const double> matrix, index_t& extent,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<double, false>(provider, matrix, extent, plan, workspace,
                                report);
}

Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix) {
  return Query<std::complex<float>, false>(provider, matrix);
}
Status Ilalc(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<float>> matrix,
             index_t& extent, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute<std::complex<float>, false>(provider, matrix, extent, plan,
                                             workspace, report);
}

Result<LapackWorkspacePlan> QueryIlalcWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix) {
  return Query<std::complex<double>, false>(provider, matrix);
}
Status Ilalc(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<double>> matrix,
             index_t& extent, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute<std::complex<double>, false>(provider, matrix, extent, plan,
                                              workspace, report);
}

Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix) {
  return Query<float, true>(provider, matrix);
}
Status Ilalr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const float> matrix, index_t& extent,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<float, true>(provider, matrix, extent, plan, workspace,
                              report);
}

Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix) {
  return Query<double, true>(provider, matrix);
}
Status Ilalr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const double> matrix, index_t& extent,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute<double, true>(provider, matrix, extent, plan, workspace,
                               report);
}

Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix) {
  return Query<std::complex<float>, true>(provider, matrix);
}
Status Ilalr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<float>> matrix,
             index_t& extent, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute<std::complex<float>, true>(provider, matrix, extent, plan,
                                            workspace, report);
}

Result<LapackWorkspacePlan> QueryIlalrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix) {
  return Query<std::complex<double>, true>(provider, matrix);
}
Status Ilalr(const ReferenceLapackProvider& provider,
             DenseBlasMatrixView<const std::complex<double>> matrix,
             index_t& extent, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute<std::complex<double>, true>(provider, matrix, extent, plan,
                                             workspace, report);
}

}  // namespace asc
