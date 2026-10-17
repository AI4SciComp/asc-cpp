#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
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
#include "asc/dense/providers/lapack_precision_conversion.h"
#include "internal_layout.h"
#include "internal_precision_conversion_counts.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace layout = internal_lapack_layout;
template <typename Input>
constexpr std::string_view Routine() {
  if constexpr (std::is_same_v<Input, float>) {
    return "slag2d";
  } else if constexpr (std::is_same_v<Input, double>) {
    return "dlag2s";
  } else if constexpr (std::is_same_v<Input, std::complex<float>>) {
    return "clag2z";
  } else {
    return "zlag2c";
  }
}
template <typename Input, typename Output>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasMatrixView<const Input> input,
                                  DenseBlasMatrixView<Output> output) {
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
  const auto kind = DenseBlasComplex<Input> ? LapackScalarKind::kMixedC128C64
                                            : LapackScalarKind::kMixedF64F32;
  const auto key = LapackPlanIdentity::Create(
      Routine<Input>(), kind,
      std::array{input.rows(), input.columns(), input.leading_dimension(),
                 output.leading_dimension()},
      std::array<std::int64_t, 2>{static_cast<std::int64_t>(input.layout()),
                                  static_cast<std::int64_t>(output.layout())},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const auto m = input.rows();
  const auto n = input.columns();
  if (m == 0 || n == 0) {
    return plan;
  }
  const auto count = internal_precision_conversion_counts::Count(
      m, n, checked::Leading(input), checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  plan.regions[checked::kScratch] = {*count, *count, sizeof(Output),
                                     alignof(Output)};
  status = layout::AddPacking(input, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}
template <typename Input, typename Output>
void Native(lapack_int m, lapack_int n, const Input* input, lapack_int leading,
            Output* output, lapack_int& info) {
  if constexpr (std::is_same_v<Input, float>) {
    LAPACK_slag2d(&m, &n, input, &leading, output, &m, &info);
  } else if constexpr (std::is_same_v<Input, double>) {
    LAPACK_dlag2s(&m, &n, input, &leading, output, &m, &info);
  } else if constexpr (std::is_same_v<Input, std::complex<float>>) {
    LAPACK_clag2z(&m, &n, input, &leading, output, &m, &info);
  } else {
    LAPACK_zlag2c(&m, &n, input, &leading, output, &m, &info);
  }
}
template <typename T>
T Sentinel() {
  const auto nan = std::numeric_limits<DenseBlasRealType<T>>::quiet_NaN();
  if constexpr (DenseBlasComplex<T>) {
    return {nan, nan};
  } else {
    return nan;
  }
}
template <typename T>
bool Publish(const T* staged, DenseBlasMatrixView<T> output) {
  bool finite = true;
  for (extent_t j = 0; j < output.columns(); ++j) {
    for (extent_t i = 0; i < output.rows(); ++i) {
      const auto value = staged[j * output.rows() + i];
      const auto offset = output.layout() == DenseBlasLayout::kColumnMajor
                              ? j * output.leading_dimension() + i
                              : i * output.leading_dimension() + j;
      output.data()[offset] = value;
      if constexpr (DenseBlasComplex<T>) {
        finite = std::isfinite(value.real()) && std::isfinite(value.imag()) &&
                 finite;
      } else {
        finite = std::isfinite(value) && finite;
      }
    }
  }
  return finite;
}
template <typename Input, typename Output>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasMatrixView<const Input> input,
               DenseBlasMatrixView<Output> output,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  checked::StartReport(provider, Routine<Input>(), report);
  const auto expected = Query(provider, input, output);
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
  if (input.rows() == 0 || input.columns() == 0) {
    return checked::Complete(report);
  }
  auto* cursor =
      static_cast<Input*>(workspace.regions[checked::kLayout].data());
  const auto* packed = layout::Pack(input, cursor);
  auto* staged =
      static_cast<Output*>(workspace.regions[checked::kScratch].data());
  std::fill_n(staged, static_cast<std::size_t>(input.rows() * input.columns()),
              Sentinel<Output>());
  lapack_int info = std::numeric_limits<lapack_int>::min();
  report.called_provider = true;
  Native(static_cast<lapack_int>(input.rows()),
         static_cast<lapack_int>(input.columns()), packed,
         static_cast<lapack_int>(checked::Leading(input)), staged, info);
  report.native_info = info;
  constexpr bool kNarrowing = sizeof(Input) > sizeof(Output);
  if (info == 1 && kNarrowing) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  if (info != 0) {
    status = checked::ProviderDefect(info, report);
    report.output_validity = LapackOutputValidity::kUnchanged;
    return status;
  }
  const bool finite = Publish(staged, output);
  status = checked::Complete(report);
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  return status;
}
}  // namespace
#define ASC_CONVERSION(NAME, INPUT, OUTPUT)                             \
  Result<LapackWorkspacePlan> Query##NAME##Workspace(                   \
      const ReferenceLapackProvider& provider,                          \
      DenseBlasMatrixView<const INPUT> input,                           \
      DenseBlasMatrixView<OUTPUT> output) {                             \
    return Query(provider, input, output);                              \
  }                                                                     \
  Status NAME(const ReferenceLapackProvider& provider,                  \
              DenseBlasMatrixView<const INPUT> input,                   \
              DenseBlasMatrixView<OUTPUT> output,                       \
              const LapackWorkspacePlan& plan,                          \
              const LapackWorkspace& workspace, LapackReport& report) { \
    return Execute(provider, input, output, plan, workspace, report);   \
  }
ASC_CONVERSION(Slag2d, float, double)
ASC_CONVERSION(Dlag2s, double, float)
ASC_CONVERSION(Clag2z, std::complex<float>, std::complex<double>)
ASC_CONVERSION(Zlag2c, std::complex<double>, std::complex<float>)
#undef ASC_CONVERSION
}  // namespace asc
