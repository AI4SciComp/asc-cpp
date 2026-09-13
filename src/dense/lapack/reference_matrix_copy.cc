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
#include "internal_layout.h"
#include "internal_matrix_copy_counts.h"
#include "internal_tridiagonal.h"
namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace layout = internal_lapack_layout;
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
    return "slacpy";
  } else if constexpr (std::is_same_v<T, double>) {
    return "dlacpy";
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    return "clacpy";
  } else {
    return "zlacpy";
  }
}
template <typename T>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  LapackMatrixPart part,
                                  DenseBlasMatrixView<const T> input,
                                  DenseBlasMatrixView<T> output) {
  if (PartCode(part) == '?') {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (input.rows() != output.rows() || input.columns() != output.columns()) {
    return Status(ErrorCode::kShape);
  }
  const std::array spans{input.reachable_storage(),
                         ConstMemoryView(output.reachable_storage())};
  Status status = checked::Disjoint(spans);
  for (const auto storage : spans) {
    if (status.ok()) {
      status = checked::Access(provider, storage);
    }
  }
  if (!status.ok()) {
    return status;
  }
  const auto key = LapackPlanIdentity::Create(
      Routine<T>(), checked::Kind<T>(),
      std::array{input.rows(), input.columns(), input.leading_dimension(),
                 output.leading_dimension()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(part),
                                  static_cast<std::int64_t>(input.layout()),
                                  static_cast<std::int64_t>(output.layout())},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  if (input.rows() == 0 || input.columns() == 0) {
    return plan;
  }
  const auto count = internal_matrix_copy_counts::Count(
      input.rows(), input.columns(), checked::Leading(input), checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  plan.regions[checked::kScratch] = {*count, *count, sizeof(T), alignof(T)};
  status = layout::AddPacking(input, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}
template <typename T>
void Native(char part, lapack_int m, lapack_int n, const T* input,
            lapack_int leading, T* output) {
  if constexpr (std::is_same_v<T, float>) {
    LAPACK_slacpy(&part, &m, &n, input, &leading, output, &m);
  } else if constexpr (std::is_same_v<T, double>) {
    LAPACK_dlacpy(&part, &m, &n, input, &leading, output, &m);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    LAPACK_clacpy(&part, &m, &n, input, &leading, output, &m);
  } else {
    LAPACK_zlacpy(&part, &m, &n, input, &leading, output, &m);
  }
}
template <typename T>
T& Entry(DenseBlasMatrixView<T> matrix, extent_t row, extent_t column) {
  return matrix.data()[matrix.layout() == DenseBlasLayout::kColumnMajor
                           ? column * matrix.leading_dimension() + row
                           : row * matrix.leading_dimension() + column];
}
template <typename T>
Status Execute(const ReferenceLapackProvider& provider, LapackMatrixPart part,
               DenseBlasMatrixView<const T> input,
               DenseBlasMatrixView<T> output, const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  checked::StartReport(provider, Routine<T>(), report);
  const auto expected = Query(provider, part, input, output);
  if (!expected.ok()) {
    return expected.status();
  }
  const std::array spans{input.reachable_storage(),
                         ConstMemoryView(output.reachable_storage())};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (status.ok()) {
    status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  }
  if (!status.ok()) {
    return status;
  }
  const auto m = input.rows();
  const auto n = input.columns();
  if (m == 0 || n == 0) {
    return checked::Complete(report);
  }
  const T* native_input = input.data();
  if (input.layout() == DenseBlasLayout::kRowMajor) {
    auto* packed = static_cast<T*>(workspace.regions[checked::kLayout].data());
    for (extent_t j = 0; j < n; ++j) {
      for (extent_t i = 0; i < m; ++i) {
        if (Selected(part, i, j)) {
          packed[j * m + i] = Entry(input, i, j);
        }
      }
    }
    native_input = packed;
  }
  auto* staged = static_cast<T*>(workspace.regions[checked::kScratch].data());
  // Initialize selected live scratch cells without inspecting caller output.
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < m; ++i) {
      if (Selected(part, i, j)) {
        staged[j * m + i] = T{};
      }
    }
  }
  report.called_provider = true;
  Native(PartCode(part), static_cast<lapack_int>(m), static_cast<lapack_int>(n),
         native_input, static_cast<lapack_int>(checked::Leading(input)),
         staged);
  for (extent_t j = 0; j < n; ++j) {
    for (extent_t i = 0; i < m; ++i) {
      if (Selected(part, i, j)) {
        Entry(output, i, j) = staged[j * m + i];
      }
    }
  }
  return checked::Complete(report);
}
}  // namespace
Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const float> input, DenseBlasMatrixView<float> output) {
  return Query(provider, part, input, output);
}
Status Lacpy(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             DenseBlasMatrixView<const float> input,
             DenseBlasMatrixView<float> output, const LapackWorkspacePlan& plan,
             const LapackWorkspace& workspace, LapackReport& report) {
  return Execute(provider, part, input, output, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const double> input,
    DenseBlasMatrixView<double> output) {
  return Query(provider, part, input, output);
}
Status Lacpy(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             DenseBlasMatrixView<const double> input,
             DenseBlasMatrixView<double> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, input, output, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const std::complex<float>> input,
    DenseBlasMatrixView<std::complex<float>> output) {
  return Query(provider, part, input, output);
}
Status Lacpy(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             DenseBlasMatrixView<const std::complex<float>> input,
             DenseBlasMatrixView<std::complex<float>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, input, output, plan, workspace, report);
}
Result<LapackWorkspacePlan> QueryLacpyWorkspace(
    const ReferenceLapackProvider& provider, LapackMatrixPart part,
    DenseBlasMatrixView<const std::complex<double>> input,
    DenseBlasMatrixView<std::complex<double>> output) {
  return Query(provider, part, input, output);
}
Status Lacpy(const ReferenceLapackProvider& provider, LapackMatrixPart part,
             DenseBlasMatrixView<const std::complex<double>> input,
             DenseBlasMatrixView<std::complex<double>> output,
             const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
             LapackReport& report) {
  return Execute(provider, part, input, output, plan, workspace, report);
}
}  // namespace asc
