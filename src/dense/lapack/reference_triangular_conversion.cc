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
#include "internal_triangular_conversion_prototypes.h"
#include "internal_tridiagonal.h"

namespace asc {
namespace {
namespace checked = internal_tridiagonal;
namespace layout = internal_lapack_layout;

template <typename Input>
constexpr std::string_view Routine() {
  return DenseBlasComplex<Input> ? "zlat2c" : "dlat2s";
}

template <typename Input, typename Output>
Result<LapackWorkspacePlan> Query(const ReferenceLapackProvider& provider,
                                  DenseBlasTriangle triangle,
                                  DenseBlasMatrixView<const Input> input,
                                  DenseBlasMatrixView<Output> output) {
  if (triangle != DenseBlasTriangle::kUpper &&
      triangle != DenseBlasTriangle::kLower) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (input.rows() != input.columns() || input.rows() != output.rows() ||
      input.columns() != output.columns()) {
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
      std::array{input.rows(), input.leading_dimension(),
                 output.leading_dimension()},
      std::array<std::int64_t, 3>{static_cast<std::int64_t>(triangle),
                                  static_cast<std::int64_t>(input.layout()),
                                  static_cast<std::int64_t>(output.layout())},
      provider.identity());
  if (!key.ok()) {
    return key.status();
  }
  LapackWorkspacePlan plan{*key};
  const auto n = input.rows();
  if (n == 0) {
    return plan;
  }
  const auto count = internal_precision_conversion_counts::Count(
      n, n, checked::Leading(input), checked::kLimit);
  if (!count.ok()) {
    return count.status();
  }
  plan.regions[checked::kScratch] = {*count, *count, sizeof(Output),
                                     alignof(Output)};
  status = layout::AddPacking(input, plan);
  return status.ok() ? Result<LapackWorkspacePlan>(plan)
                     : Result<LapackWorkspacePlan>(status);
}

template <typename Function>
void EachEntry(extent_t n, DenseBlasTriangle triangle, Function&& function) {
  for (extent_t j = 0; j < n; ++j) {
    const auto first = triangle == DenseBlasTriangle::kUpper ? 0 : j;
    const auto last = triangle == DenseBlasTriangle::kUpper ? j + 1 : n;
    for (extent_t i = first; i < last; ++i) {
      function(i, j);
    }
  }
}

template <typename Input>
const Input* Pack(DenseBlasMatrixView<const Input> input,
                  DenseBlasTriangle triangle,
                  const LapackWorkspace& workspace) {
  if (input.layout() == DenseBlasLayout::kColumnMajor) {
    return input.data();
  }
  auto* packed =
      static_cast<Input*>(workspace.regions[checked::kLayout].data());
  EachEntry(input.rows(), triangle, [&](extent_t i, extent_t j) {
    packed[j * input.rows() + i] =
        input.data()[i * input.leading_dimension() + j];
  });
  return packed;
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

template <typename Input, typename Output>
lapack_int Native(DenseBlasTriangle triangle, lapack_int n, const Input* input,
                  lapack_int leading, Output* output) {
  const char uplo = triangle == DenseBlasTriangle::kUpper ? 'U' : 'L';
  // Both pinned LAT2 definitions write INFO only on overflow. Zero is an input
  // requirement of this adapter, not evidence of a provider success write.
  lapack_int info = 0;
  if constexpr (std::is_same_v<Input, double>) {
    LAPACK_dlat2s_base(&uplo, &n, input, &leading, output, &n, &info,
                       std::size_t{1});
  } else {
    LAPACK_zlat2c_base(&uplo, &n, input, &leading, output, &n, &info,
                       std::size_t{1});
  }
  return info;
}

template <typename Output>
bool Publish(const Output* staged, DenseBlasMatrixView<Output> output,
             DenseBlasTriangle triangle) {
  bool finite = true;
  EachEntry(output.rows(), triangle, [&](extent_t i, extent_t j) {
    const auto value = staged[j * output.rows() + i];
    const auto offset = output.layout() == DenseBlasLayout::kColumnMajor
                            ? j * output.leading_dimension() + i
                            : i * output.leading_dimension() + j;
    output.data()[offset] = value;
    if constexpr (DenseBlasComplex<Output>) {
      finite =
          std::isfinite(value.real()) && std::isfinite(value.imag()) && finite;
    } else {
      finite = std::isfinite(value) && finite;
    }
  });
  return finite;
}

template <typename Input, typename Output>
Status Execute(const ReferenceLapackProvider& provider,
               DenseBlasTriangle triangle,
               DenseBlasMatrixView<const Input> input,
               DenseBlasMatrixView<Output> output,
               const LapackWorkspacePlan& plan,
               const LapackWorkspace& workspace, LapackReport& report) {
  const std::array spans{input.reachable_storage(),
                         ConstMemoryView(output.reachable_storage())};
  Status status =
      checked::CheckMetadata(provider, plan, workspace, report, spans);
  if (!status.ok()) {
    return status;
  }
  checked::StartReport(provider, Routine<Input>(), report);
  const auto expected = Query(provider, triangle, input, output);
  if (!expected.ok()) {
    return expected.status();
  }
  status = checked::ValidatePlan(provider, *expected, plan, workspace, spans);
  if (!status.ok()) {
    return status;
  }
  if (input.rows() == 0) {
    return checked::Complete(report);
  }
  const auto* packed = Pack(input, triangle, workspace);
  auto* staged =
      static_cast<Output*>(workspace.regions[checked::kScratch].data());
  std::fill_n(staged, static_cast<std::size_t>(input.rows() * input.rows()),
              Sentinel<Output>());
  report.called_provider = true;
  const auto info =
      Native(triangle, static_cast<lapack_int>(input.rows()), packed,
             static_cast<lapack_int>(checked::Leading(input)), staged);
  report.native_info = info;
  if (info == 1) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  if (info != 0) {
    status = checked::ProviderDefect(info, report);
    report.output_validity = LapackOutputValidity::kUnchanged;
    return status;
  }
  const bool finite = Publish(staged, output, triangle);
  status = checked::Complete(report);
  if (!finite) {
    report.outcome = LapackOutcome::kAccuracyWarning;
    return Status(ErrorCode::kNumerical);
  }
  return status;
}
}  // namespace

#define ASC_TRIANGULAR_CONVERSION(NAME, INPUT, OUTPUT)                     \
  Result<LapackWorkspacePlan> Query##NAME##Workspace(                      \
      const ReferenceLapackProvider& provider, DenseBlasTriangle triangle, \
      DenseBlasMatrixView<const INPUT> input,                              \
      DenseBlasMatrixView<OUTPUT> output) {                                \
    return Query(provider, triangle, input, output);                       \
  }                                                                        \
  Status NAME(                                                             \
      const ReferenceLapackProvider& provider, DenseBlasTriangle triangle, \
      DenseBlasMatrixView<const INPUT> input,                              \
      DenseBlasMatrixView<OUTPUT> output, const LapackWorkspacePlan& plan, \
      const LapackWorkspace& workspace, LapackReport& report) {            \
    return Execute(provider, triangle, input, output, plan, workspace,     \
                   report);                                                \
  }
ASC_TRIANGULAR_CONVERSION(Dlat2s, double, float)
ASC_TRIANGULAR_CONVERSION(Zlat2c, std::complex<double>, std::complex<float>)
#undef ASC_TRIANGULAR_CONVERSION
}  // namespace asc
