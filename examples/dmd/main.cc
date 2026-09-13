#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_dmd.h"

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
template <typename T>
using Real = asc::DenseBlasRealType<T>;
template <typename T>
T Eigenvalue(int i) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real<T>>(2 + i), static_cast<Real<T>>(1 - 2 * i)};
  } else {
    return static_cast<T>(2 + i);
  }
}
template <typename T>
struct Matrix {
  std::array<T, 16> values{};
  asc::DenseBlasLayout layout;
  explicit Matrix(asc::DenseBlasLayout selected) : layout(selected) {
    values.fill(T{-17});
  }
  T& At(int i, int j) {
    return values[1 + static_cast<std::size_t>(layout == kColumn ? j * 4 + i
                                                                 : i * 4 + j)];
  }
  auto View(int rows) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 1, rows, 2, layout, 4,
        {values.data(), sizeof(values), kHost}));
  }
};
template <typename T>
auto Vector(std::array<T, 2>& values) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data(), 2, 1, {values.data(), sizeof(values), kHost}));
}
template <typename T>
struct Scratch {
  std::vector<T> scalar, layout;
  std::vector<Real<T>> real, scratch;
  std::vector<std::max_align_t> integer;
  asc::LapackWorkspace workspace;
  Scratch(const asc::LapackWorkspacePlan& plan, bool preferred) {
    const auto set = [&](asc::LapackWorkspaceKind role, auto& storage) {
      const auto index = static_cast<std::size_t>(role);
      const auto& region = plan.regions[index];
      const auto count =
          preferred ? region.preferred_entries : region.minimum_entries;
      const auto bytes = static_cast<std::size_t>(count) * region.entry_bytes;
      const auto entry = sizeof(storage[0]);
      storage.resize((bytes + entry - 1) / entry);
      if (bytes != 0) {
        workspace.regions[index] = {storage.data(), bytes, kHost};
      }
    };
    set(asc::LapackWorkspaceKind::kScalar, scalar);
    set(asc::LapackWorkspaceKind::kLayoutConversion, layout);
    set(asc::LapackWorkspaceKind::kReal, real);
    set(asc::LapackWorkspaceKind::kScratch, scratch);
    set(asc::LapackWorkspaceKind::kInteger, integer);
  }
};
template <typename T>
void ResetSnapshots(Matrix<T>& x, Matrix<T>& y) {
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 3; ++i) {
      x.At(i, j) = i == j ? T{1} : T{0};
      y.At(i, j) = i == j ? Eigenvalue<T>(i) : T{0};
    }
  }
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, asc::LapackDmdSvd svd,
         asc::LapackDmdVectors vectors, asc::DenseBlasLayout input_layout,
         bool preferred) {
  const auto output_layout = input_layout == kColumn ? kRow : kColumn;
  Matrix<T> x(input_layout);
  Matrix<T> y(output_layout);
  Matrix<T> z(output_layout);
  Matrix<T> b(input_layout);
  Matrix<T> w(input_layout);
  Matrix<T> s(output_layout);
  std::array<std::complex<Real<T>>, 2> eigen{};
  std::array<Real<T>, 2> singular{};
  std::array<Real<T>, 2> residual{};
  const asc::LapackDmdOptions options{
      asc::LapackDmdScaling::kNone,
      vectors,
      asc::LapackDmdExtra::kRefinement,
      svd,
      vectors == asc::LapackDmdVectors::kExplicit,
      -1};
  const asc::LapackDmdBuffers<T> buffers{
      x.View(3), y.View(3),     z.View(3),        b.View(3),       w.View(2),
      s.View(2), Vector(eigen), Vector(singular), Vector(residual)};
  const auto plan =
      Take(asc::QueryGedmdWorkspace(provider, options, buffers, Real<T>{0}));
  Scratch<T> storage(plan, preferred);
  const auto tolerance = Real<T>{128} * std::numeric_limits<Real<T>>::epsilon();
  for (int reuse = 0; reuse < 2; ++reuse) {
    ResetSnapshots(x, y);
    asc::index_t rank = -1;
    asc::LapackReport report;
    if (!asc::Gedmd(provider, options, buffers, Real<T>{0}, rank, plan,
                    storage.workspace, report)
             .ok() ||
        !report.called_provider || report.native_info != 0 || rank != 2) {
      return false;
    }
    for (int j = 0; j < 2; ++j) {
      const auto column = static_cast<std::size_t>(j);
      if (std::abs(singular[column] - Real<T>{1}) > tolerance ||
          std::min(std::abs(eigen[column] - Eigenvalue<T>(0)),
                   std::abs(eigen[column] - Eigenvalue<T>(1))) > tolerance) {
        return false;
      }
      Real<T> norm{};
      for (int i = 0; i < 3; ++i) {
        T mode = z.At(i, j);
        if (vectors == asc::LapackDmdVectors::kFactored) {
          mode = x.At(i, 0) * w.At(0, j) + x.At(i, 1) * w.At(1, j);
        }
        const std::complex<Real<T>> wide = mode;
        norm += std::norm(wide);
        const auto diagonal = i < 2 ? Eigenvalue<T>(i) : T{0};
        if (std::abs((diagonal - eigen[column]) * wide) > tolerance ||
            std::abs(b.At(i, j) - diagonal * x.At(i, j)) > tolerance) {
          return false;
        }
      }
      if (std::abs(norm - Real<T>{1}) > tolerance ||
          (options.residuals && std::abs(residual[column]) > tolerance)) {
        return false;
      }
    }
    if (std::abs(eigen[0] - eigen[1]) < Real<T>{0.5}) {
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
  for (auto svd :
       {asc::LapackDmdSvd::kBidiagonalQr, asc::LapackDmdSvd::kDivideAndConquer,
        asc::LapackDmdSvd::kQrPreconditioned, asc::LapackDmdSvd::kJacobi}) {
    for (auto vectors :
         {asc::LapackDmdVectors::kExplicit, asc::LapackDmdVectors::kFactored}) {
      for (auto layout : {kColumn, kRow}) {
        for (bool preferred : {false, true}) {
          passed =
              Run<float>(provider, svd, vectors, layout, preferred) && passed;
          passed =
              Run<double>(provider, svd, vectors, layout, preferred) && passed;
          passed = Run<std::complex<float>>(provider, svd, vectors, layout,
                                            preferred) &&
                   passed;
          passed = Run<std::complex<double>>(provider, svd, vectors, layout,
                                             preferred) &&
                   passed;
        }
      }
    }
  }
  g_returned = true;
  std::puts(passed ? "Public GEDMD eigenpairs, POD factors, diagnostics and "
                     "plan reuse passed"
                   : "GEDMD check failed");
  return passed ? 0 : 1;
}
