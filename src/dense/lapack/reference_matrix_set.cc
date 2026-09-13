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
#include "asc/dense/providers/lapack_matrix_copy.h"
#include "asc/dense/providers/lapack_matrix_set.h"
#include "internal_matrix_copy_counts.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
namespace checked = internal_tridiagonal;
char PartCode(LapackMatrixPart part) {
  switch (part) {
    case LapackMatrixPart::kAll:
      return 'A';
    case LapackMatrixPart::kUpper:
      return 'U';
    case LapackMatrixPart::kLower:
      return 'L';
  }
  return '?';
}
bool Selected(LapackMatrixPart part, extent_t row, extent_t column) {
  return part == LapackMatrixPart::kAll ||
         (part == LapackMatrixPart::kUpper && row <= column) ||
         (part == LapackMatrixPart::kLower && row >= column);
}
template <typename T>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<T, float>) {
    return "slaset";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dlaset";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "claset";
  } else {
    return "zlaset";
  }
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackMatrixPart part,
                                  DenseBlasMatrixView<T> output) {
  if (PartCode(part) == '?') {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto status = checked::Access(provider, output.reachable_storage());
  if (!status.ok()) {
    return status;
  }
  const auto identity = LapackPlanIdentity::Create(
      Routine<T>(), checked::Kind<T>(),
      std::array{output.rows(), output.columns(), output.leading_dimension()},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(part),
                                  static_cast<std::int64_t>(output.layout())},
      provider.identity());
  if (!identity.ok()) {
    return identity.status();
  }
  LapackWorkspacePlan plan{*identity};
  if (output.rows() == 0 || output.columns() == 0) {
    return plan;
  }
  // LASET and LACPY share the full-matrix terminal-index/count bounds.
  const auto count = internal_matrix_copy_counts::Count(
      output.rows(), output.columns(), checked::Leading(output),
      checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  if (output.layout() == DenseBlasLayout::kRowMajor) {
    plan.regions[checked::kLayout] = {*count, *count, sizeof(T), alignof(T)};
  }
  return plan;
}
template <typename T>
void Native(char part, lapack_int m, lapack_int n, T alpha, T beta, T* output,
            lapack_int leading) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_slaset(&part, &m, &n, &alpha, &beta, output, &leading);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dlaset(&part, &m, &n, &alpha, &beta, output, &leading);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_claset(&part, &m, &n, &alpha, &beta, output, &leading);
  } else {
    LAPACK_zlaset(&part, &m, &n, &alpha, &beta, output, &leading);
  }
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, LapackMatrixPart part,
               T alpha, T beta, DenseBlasMatrixView<T> output,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, part, output);
  if (!expected.ok()) {
    return expected.status();
  }
  const std::array spans{ConstMemoryView(output.reachable_storage())};
  auto status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (status.ok()) {
    status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  }
  if (!status.ok()) {
    return status;
  }
  const auto m = output.rows();
  const auto n = output.columns();
  if (m == 0 || n == 0) {
    return checked::Complete(report);
  }
  const bool staged = output.layout() == DenseBlasLayout::kRowMajor;
  auto* native_output =
      staged ? static_cast<T*>(workspace.regions[checked::kLayout].data())
             : output.data();
  // Native assignment requires live scalar storage but no initial numeric
  // value. No caller output or scratch value is read before the actual foreign
  // entry.
  report.called_provider = true;
  Native(PartCode(part), static_cast<lapack_int>(m), static_cast<lapack_int>(n),
         alpha, beta, native_output,
         static_cast<lapack_int>(staged ? m : output.leading_dimension()));
  if (staged) {
    for (extent_t j = 0; j < n; ++j) {
      for (extent_t i = 0; i < m; ++i) {
        if (Selected(part, i, j)) {
          output.data()[i * output.leading_dimension() + j] =
              native_output[j * m + i];
        }
      }
    }
  }
  return checked::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<float> output) {
  return Query(provider, part, output);
}
Status Laset(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             float alpha, float beta, DenseBlasMatrixView<float> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, alpha, beta, output, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<double> output) {
  return Query(provider, part, output);
}
Status Laset(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             double alpha, double beta, DenseBlasMatrixView<double> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, alpha, beta, output, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<std::complex<float>> output) {
  return Query(provider, part, output);
}
Status Laset(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             std::complex<float> alpha, std::complex<float> beta,
             DenseBlasMatrixView<std::complex<float>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, alpha, beta, output, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLasetWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<std::complex<double>> output) {
  return Query(provider, part, output);
}
Status Laset(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             std::complex<double> alpha, std::complex<double> beta,
             DenseBlasMatrixView<std::complex<double>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, alpha, beta, output, plan, workspace, report);
}
}  // namespace asc
