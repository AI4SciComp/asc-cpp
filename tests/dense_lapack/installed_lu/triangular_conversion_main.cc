#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_precision_conversion.h"
#include "normal_return_guard.h"
namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kScratch =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}
template <typename Input>
using Output = std::conditional_t<std::is_same_v<Input, double>, float,
                                  std::complex<float>>;
template <typename T>
T Value(int i, int j, int shift) {
  using Real = asc::DenseBlasRealType<T>;
  const auto real = static_cast<Real>((i - shift) * 0.125 + j * 0.25);
  if constexpr (asc::DenseBlasComplex<T>) {
    return {real, Real{0.5}};
  } else {
    return real;
  }
}
template <typename Input>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle,
           asc::DenseBlasMatrixView<const Input> input,
           asc::DenseBlasMatrixView<Output<Input>> output) {
  if constexpr (std::is_same_v<Input, double>) {
    return asc::QueryDlat2sWorkspace(provider, triangle, input, output);
  } else {
    return asc::QueryZlat2cWorkspace(provider, triangle, input, output);
  }
}
template <typename Input>
auto Execute(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasTriangle triangle,
             asc::DenseBlasMatrixView<const Input> input,
             asc::DenseBlasMatrixView<Output<Input>> output,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (std::is_same_v<Input, double>) {
    return asc::Dlat2s(provider, triangle, input, output, plan, workspace,
                       report);
  } else {
    return asc::Zlat2c(provider, triangle, input, output, plan, workspace,
                       report);
  }
}
std::size_t Index(int i, int j, int n, asc::DenseBlasLayout layout) {
  const auto row = static_cast<std::size_t>(i);
  const auto column = static_cast<std::size_t>(j);
  const auto leading = static_cast<std::size_t>(n) + 3;
  return 1 +
         (layout == kColumn ? column * leading + row : row * leading + column);
}
bool Selected(asc::DenseBlasTriangle triangle, int i, int j) {
  return triangle == kUpper ? i <= j : i >= j;
}
template <typename Input>
struct Storage {
  std::array<Input, 256> input;
  std::array<Output<Input>, 256> output;
  std::array<Input, 256> packing;
  std::array<Output<Input>, 256> staging;
  Storage() {
    input.fill(Input{-17});
    output.fill(Output<Input>{-19});
    packing.fill(Input{-23});
    staging.fill(Output<Input>{-29});
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace work;
    const auto count =
        static_cast<std::size_t>(plan.regions[kScratch].minimum_entries);
    if (count != 0) {
      work.regions[kScratch] = {staging.data() + 1, count * sizeof(staging[0]),
                                kHost};
      if (plan.regions[kLayout].minimum_entries != 0) {
        work.regions[kLayout] = {packing.data() + 1, count * sizeof(packing[0]),
                                 kHost};
      }
    }
    return work;
  }
  [[nodiscard]] bool Guards(const asc::LapackWorkspace& work) const {
    const auto scratch_end =
        1 + work.regions[kScratch].size() / sizeof(staging[0]);
    const auto packed_end =
        1 + work.regions[kLayout].size() / sizeof(packing[0]);
    for (std::size_t i = 0; i < staging.size(); ++i) {
      if ((i == 0 || i >= scratch_end) && staging[i] != Output<Input>{-29}) {
        return false;
      }
      if ((i == 0 || i >= packed_end) && packing[i] != Input{-23}) {
        return false;
      }
    }
    return true;
  }
};
template <typename Input>
bool Run(const asc::ReferenceLapackProvider& provider,
         asc::DenseBlasTriangle triangle, asc::DenseBlasLayout il,
         asc::DenseBlasLayout ol, int n, int shift) {
  Storage<Input> storage;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      storage.input[Index(i, j, n, il)] =
          Selected(triangle, i, j) ? Value<Input>(i, j, shift)
                                   : Input{std::numeric_limits<double>::max()};
    }
  }
  const auto before = storage.input;
  const auto iv = Take(asc::DenseBlasMatrixView<const Input>::Create(
      storage.input.data() + 1, n, n, il, n + 3,
      {storage.input.data(), sizeof(storage.input), kHost}));
  const auto ov = Take(asc::DenseBlasMatrixView<Output<Input>>::Create(
      storage.output.data() + 1, n, n, ol, n + 3,
      {storage.output.data(), sizeof(storage.output), kHost}));
  const auto plan = Take(Query<Input>(provider, triangle, iv, ov));
  const auto workspace = storage.Workspace(plan);
  asc::LapackReport report;
  for (int reuse = 0; reuse < 2; ++reuse) {
    if (!Execute<Input>(provider, triangle, iv, ov, plan, workspace, report)
             .ok() ||
        report.called_provider != (n > 0) ||
        report.native_info.has_value() != (n > 0) ||
        (n > 0 && report.native_info != 0) || storage.input != before) {
      return false;
    }
    auto wanted = storage.output;
    wanted.fill(Output<Input>{-19});
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        if (Selected(triangle, i, j)) {
          wanted[Index(i, j, n, ol)] = Value<Output<Input>>(i, j, shift);
        }
      }
    }
    if (storage.output != wanted || !storage.Guards(workspace)) {
      return false;
    }
  }
  const auto old_output = storage.output;
  const auto opposite = triangle == kUpper ? kLower : kUpper;
  if (Execute<Input>(provider, opposite, iv, ov, plan, workspace, report)
              .code() != asc::ErrorCode::kInvalidState ||
      report.called_provider || storage.output != old_output) {
    return false;
  }
  if (n > 0) {
    storage.input[Index(n - 1, n - 1, n, il)] =
        Input{std::numeric_limits<double>::max()};
    if (Execute<Input>(provider, triangle, iv, ov, plan, workspace, report)
                .code() != asc::ErrorCode::kNumerical ||
        report.native_info != 1 || storage.output != old_output ||
        report.output_validity != asc::LapackOutputValidity::kUnchanged) {
      return false;
    }
  }
  return storage.Guards(workspace);
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  bool passed = true;
  for (auto triangle : {kUpper, kLower}) {
    for (auto il : {kColumn, kRow}) {
      for (auto ol : {kColumn, kRow}) {
        for (int n : {0, 1, 2, 5, 13}) {
          for (int shift = 0; shift < 16; ++shift) {
            passed =
                Run<double>(provider, triangle, il, ol, n, shift) && passed;
            passed = Run<std::complex<double>>(provider, triangle, il, ol, n,
                                               shift) &&
                     passed;
          }
        }
      }
    }
  }
  std::puts(passed ? "1280 public triangular conversion cases passed"
                   : "Triangular conversion failed");
  return passed ? 0 : 1;
}
