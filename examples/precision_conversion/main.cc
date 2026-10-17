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
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_precision_conversion.h"
namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
bool g_returned = false;
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}
template <typename Input>
using Output = std::conditional_t<
    std::is_same_v<Input, float>, double,
    std::conditional_t<
        std::is_same_v<Input, double>, float,
        std::conditional_t<std::is_same_v<Input, std::complex<float>>,
                           std::complex<double>, std::complex<float>>>>;
template <typename T>
T Value(int i, int j) {
  using Real = asc::DenseBlasRealType<T>;
  const auto real = static_cast<Real>(i * 0.125 + j * 0.25);
  if constexpr (asc::DenseBlasComplex<T>) {
    return {real, Real{0.5}};
  } else {
    return real;
  }
}
template <typename Input>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasMatrixView<const Input> input,
           asc::DenseBlasMatrixView<Output<Input>> output) {
  if constexpr (std::is_same_v<Input, float>) {
    return asc::QuerySlag2dWorkspace(provider, input, output);
  } else if constexpr (std::is_same_v<Input, double>) {
    return asc::QueryDlag2sWorkspace(provider, input, output);
  } else if constexpr (std::is_same_v<Input, std::complex<float>>) {
    return asc::QueryClag2zWorkspace(provider, input, output);
  } else {
    return asc::QueryZlag2cWorkspace(provider, input, output);
  }
}
template <typename Input>
auto Execute(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasMatrixView<const Input> input,
             asc::DenseBlasMatrixView<Output<Input>> output,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (std::is_same_v<Input, float>) {
    return asc::Slag2d(provider, input, output, plan, workspace, report);
  } else if constexpr (std::is_same_v<Input, double>) {
    return asc::Dlag2s(provider, input, output, plan, workspace, report);
  } else if constexpr (std::is_same_v<Input, std::complex<float>>) {
    return asc::Clag2z(provider, input, output, plan, workspace, report);
  } else {
    return asc::Zlag2c(provider, input, output, plan, workspace, report);
  }
}
std::size_t Index(int i, int j, asc::DenseBlasLayout layout) {
  const auto row = static_cast<std::size_t>(i);
  const auto column = static_cast<std::size_t>(j);
  return 1 + (layout == kColumn ? column * 5 + row : row * 5 + column);
}
template <typename Input>
bool Run(const asc::ReferenceLapackProvider& provider, asc::DenseBlasLayout il,
         asc::DenseBlasLayout ol) {
  std::array<Input, 32> input{};
  std::array<Output<Input>, 32> output{};
  input.fill(Input{-17});
  output.fill(Output<Input>{-19});
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      input[Index(i, j, il)] = Value<Input>(i, j);
    }
  }
  const auto before = input;
  const auto iv = Take(asc::DenseBlasMatrixView<const Input>::Create(
      input.data() + 1, 3, 2, il, 5, {input.data(), sizeof(input), kHost}));
  const auto ov = Take(asc::DenseBlasMatrixView<Output<Input>>::Create(
      output.data() + 1, 3, 2, ol, 5, {output.data(), sizeof(output), kHost}));
  const auto plan = Take(Query<Input>(provider, iv, ov));
  std::array<Input, 6> packed{};
  std::array<Output<Input>, 6> staged{};
  asc::LapackWorkspace workspace;
  const auto layout =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
  const auto scratch =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
  if (il == kRow) {
    workspace.regions[layout] = {packed.data(), sizeof(packed), kHost};
  }
  workspace.regions[scratch] = {staged.data(), sizeof(staged), kHost};
  asc::LapackReport report;
  for (int reuse = 0; reuse < 2; ++reuse) {
    if (!Execute<Input>(provider, iv, ov, plan, workspace, report).ok() ||
        !report.called_provider || report.native_info != 0 || input != before) {
      return false;
    }
    auto guards = output;
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 2; ++j) {
        if (output[Index(i, j, ol)] != Value<Output<Input>>(i, j)) {
          return false;
        }
        guards[Index(i, j, ol)] = Output<Input>{-19};
      }
    }
    for (const auto value : guards) {
      if (value != Output<Input>{-19}) {
        return false;
      }
    }
  }
  if constexpr (sizeof(Input) > sizeof(Output<Input>)) {
    const auto old_output = output;
    input[Index(2, 1, il)] = Input{2.0 * std::numeric_limits<float>::max()};
    const auto status =
        Execute<Input>(provider, iv, ov, plan, workspace, report);
    if (status.ok() || report.native_info != 1 || output != old_output ||
        report.output_validity != asc::LapackOutputValidity::kUnchanged) {
      return false;
    }
  }
  return true;
}
}  // namespace
int main() {
  if (std::atexit([] {
        if (!g_returned) {
          std::_Exit(93);
        }
      }) != 0) {
    return 92;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  bool passed = true;
  for (auto il : {kColumn, kRow}) {
    for (auto ol : {kColumn, kRow}) {
      passed = Run<float>(provider, il, ol) && passed;
      passed = Run<double>(provider, il, ol) && passed;
      passed = Run<std::complex<float>>(provider, il, ol) && passed;
      passed = Run<std::complex<double>>(provider, il, ol) && passed;
    }
  }
  g_returned = true;
  std::puts(passed
                ? "Four public precision conversions and range reports passed"
                : "Conversion check failed");
  return passed ? 0 : 1;
}
