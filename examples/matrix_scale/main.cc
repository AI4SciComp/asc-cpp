#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_scale.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
using Part = asc::LapackMatrixScalePart;
bool g_returned = false;

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}

bool Selected(char type, int i, int j) {
  switch (type) {
    case 'G':
      return true;
    case 'L':
      return i >= j;
    case 'U':
      return i <= j;
    case 'H':
      return i <= j + 1;
    case 'B':
      return i >= j && i - j <= 1;
    case 'Q':
      return j >= i && j - i <= 1;
    default:
      return i - j <= 1 && j - i <= 1;
  }
}

std::size_t Index(char type, int i, int j, asc::DenseBlasLayout layout) {
  int offset = layout == kColumn ? j * 6 + i : i * 6 + j;
  if (type == 'B') {
    offset = layout == kColumn ? j * 6 + i - j : i * 6 + 1 + j - i;
  } else if (type == 'Q') {
    offset = layout == kColumn ? j * 6 + 1 + i - j : i * 6 + j - i;
  } else if (type == 'Z') {
    offset = j * 6 + 2 + i - j;
  }
  return 1 + static_cast<std::size_t>(offset);
}

Part MatrixPart(char type) {
  switch (type) {
    case 'G':
      return Part::kAll;
    case 'L':
      return Part::kLower;
    case 'U':
      return Part::kUpper;
    default:
      return Part::kUpperHessenberg;
  }
}

template <typename T>
struct Example {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 32> matrix{};
  char type;
  asc::DenseBlasLayout layout;

  auto Full() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        matrix.data() + 1, 3, 3, layout, 6,
        {matrix.data(), sizeof(matrix), kHost}));
  }
  auto PositiveBand() {
    return Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        matrix.data() + 1, 3, 1,
        type == 'B' ? asc::DenseBlasTriangle::kLower
                    : asc::DenseBlasTriangle::kUpper,
        layout, 6, {matrix.data(), sizeof(matrix), kHost}));
  }
  auto GeneralBand() {
    return Take(asc::LapackLuBandView<T>::Create(
        matrix.data() + 1, 3, 3, 1, 1, 6,
        {matrix.data(), sizeof(matrix), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if (type == 'Z') {
      return asc::QueryLasclWorkspace(provider, GeneralBand());
    }
    if (type == 'B' || type == 'Q') {
      return asc::QueryLasclWorkspace(provider, PositiveBand());
    }
    return asc::QueryLasclWorkspace(provider, MatrixPart(type), Full());
  }
  auto Scale(const asc::ReferenceLapackProvider& provider, Real from, Real to,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
    if (type == 'Z') {
      return asc::Lascl(provider, from, to, GeneralBand(), plan, workspace,
                        report);
    }
    if (type == 'B' || type == 'Q') {
      return asc::Lascl(provider, from, to, PositiveBand(), plan, workspace,
                        report);
    }
    return asc::Lascl(provider, MatrixPart(type), from, to, Full(), plan,
                      workspace, report);
  }
};

template <typename T>
T Coefficient(int i, int j) {
  using Real = asc::DenseBlasRealType<T>;
  const Real real = i == j ? Real{4} : Real{1};
  if constexpr (asc::DenseBlasComplex<T>) {
    Real imaginary = 0;
    if (i != j) {
      imaginary = i > j ? Real{1} : Real{-1};
    }
    return {real, imaginary};
  } else {
    return real;
  }
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, char type,
         asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  Example<T> example{.type = type, .layout = layout};
  example.matrix.fill(T{-19});
  auto expected = example.matrix;
  constexpr Real kTiny = std::numeric_limits<Real>::denorm_min();
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (Selected(type, i, j)) {
        const auto at = Index(type, i, j, layout);
        expected[at] = Coefficient<T>(i, j);
        example.matrix[at] = expected[at] * kTiny;
      }
    }
  }
  const auto plan = Take(example.Query(provider));
  std::array<T, 32> packing{};
  asc::LapackWorkspace workspace;
  const auto kind =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
  if (layout == kRow) {
    workspace.regions[kind] = {packing.data(), sizeof(packing), kHost};
  }
  asc::LapackReport report;
  // The caller never forms 1/kTiny, which overflows in the working precision.
  if (!example.Scale(provider, kTiny, Real{1}, plan, workspace, report).ok() ||
      !report.called_provider || report.native_info.value_or(-1) != 0 ||
      example.matrix != expected) {
    return false;
  }
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (Selected(type, i, j)) {
        expected[Index(type, i, j, layout)] *= Real{-2};
      }
    }
  }
  // Plans bind storage metadata; factors can change on documented reuse.
  return example.Scale(provider, Real{1}, Real{-2}, plan, workspace, report)
             .ok() &&
         report.called_provider && report.native_info.value_or(-1) == 0 &&
         example.matrix == expected;
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
  for (auto layout : {kColumn, kRow}) {
    for (char type : {'G', 'L', 'U', 'H', 'B', 'Q', 'Z'}) {
      if (type == 'Z' && layout == kRow) {
        continue;  // The public general-band descriptor is column-only.
      }
      passed = Run<float>(provider, type, layout) && passed;
      passed = Run<double>(provider, type, layout) && passed;
      passed = Run<std::complex<float>>(provider, type, layout) && passed;
      passed = Run<std::complex<double>>(provider, type, layout) && passed;
    }
  }
  g_returned = true;
  std::puts(passed ? "Four public LASCL scalars, seven storage modes, safe "
                     "scaling, changed-factor plan reuse and guards passed"
                   : "LASCL check failed");
  return passed ? 0 : 1;
}
