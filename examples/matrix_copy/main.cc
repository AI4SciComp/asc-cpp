#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
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
using Output = Input;
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
           asc::LapackMatrixPart part,
           asc::DenseBlasMatrixView<const Input> input,
           asc::DenseBlasMatrixView<Input> output) {
  return asc::QueryLacpyWorkspace(provider, part, input, output);
}
template <typename Input>
auto Execute(const asc::ReferenceLapackProvider& provider,
             asc::LapackMatrixPart part,
             asc::DenseBlasMatrixView<const Input> input,
             asc::DenseBlasMatrixView<Input> output,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  return asc::Lacpy(provider, part, input, output, plan, workspace, report);
}
std::size_t Index(int i, int j, asc::DenseBlasLayout layout) {
  const auto row = static_cast<std::size_t>(i);
  const auto column = static_cast<std::size_t>(j);
  return 1 + (layout == kColumn ? column * 5 + row : row * 5 + column);
}
template <typename Input>
bool Run(const asc::ReferenceLapackProvider& provider,
         asc::LapackMatrixPart part, asc::DenseBlasLayout il,
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
  const auto plan = Take(Query<Input>(provider, part, iv, ov));
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
    if (!Execute<Input>(provider, part, iv, ov, plan, workspace, report).ok() ||
        !report.called_provider || report.native_info.has_value() ||
        input != before) {
      return false;
    }
    auto guards = output;
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 2; ++j) {
        const bool selected =
            part == asc::LapackMatrixPart::kAll ||
            (part == asc::LapackMatrixPart::kUpper ? i <= j : i >= j);
        const auto expected = selected ? Value<Input>(i, j) : Input{-19};
        if (output[Index(i, j, ol)] != expected) {
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
      for (auto part :
           {asc::LapackMatrixPart::kAll, asc::LapackMatrixPart::kUpper,
            asc::LapackMatrixPart::kLower}) {
        passed = Run<float>(provider, part, il, ol) && passed;
        passed = Run<double>(provider, part, il, ol) && passed;
        passed = Run<std::complex<float>>(provider, part, il, ol) && passed;
        passed = Run<std::complex<double>>(provider, part, il, ol) && passed;
      }
    }
  }
  g_returned = true;
  std::puts(passed ? "Four public LACPY copies, three parts, independent "
                     "layouts and absent INFO passed"
                   : "LACPY check failed");
  return passed ? 0 : 1;
}
