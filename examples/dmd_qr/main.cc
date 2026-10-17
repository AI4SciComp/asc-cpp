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
#include "asc/dense/providers/lapack_dmd_qr.h"
#include "asc/dense/providers/lapack_qr.h"

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
  std::array<T, 64> values{};
  asc::DenseBlasLayout layout;
  explicit Matrix(asc::DenseBlasLayout selected) : layout(selected) {
    values.fill(T{-17});
  }
  T& At(int i, int j) {
    return values[1 + static_cast<std::size_t>(layout == kColumn ? j * 6 + i
                                                                 : i * 6 + j)];
  }
  [[nodiscard]] T At(int i, int j) const {
    return values[1 + static_cast<std::size_t>(layout == kColumn ? j * 6 + i
                                                                 : i * 6 + j)];
  }
  [[nodiscard]] auto View(int rows, int columns) const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        values.data() + 1, rows, columns, layout, 6,
        {values.data(), sizeof(values), kHost}));
  }
  auto View(int rows, int columns) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 1, rows, columns, layout, 6,
        {values.data(), sizeof(values), kHost}));
  }
};
template <typename T, std::size_t N>
auto Vector(std::array<T, N>& values) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data(), N, 1, {values.data(), sizeof(values), kHost}));
}
template <typename T, std::size_t N>
auto Vector(const std::array<T, N>& values) {
  return Take(asc::DenseBlasVectorView<const T>::Create(
      values.data(), N, 1, {values.data(), sizeof(values), kHost}));
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
bool FormQ(const asc::ReferenceLapackProvider& provider, const Matrix<T>& f,
           const std::array<T, 3>& tau, Matrix<T>& q, bool explicit_q) {
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 4; ++i) {
      q.At(i, j) = explicit_q ? f.At(i, j) : T{static_cast<Real<T>>(i == j)};
    }
  }
  if (explicit_q) {
    return true;
  }
  asc::LapackReport report;
  const auto query = [&] {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::QueryUnmqrWorkspace(
          provider, asc::DenseBlasSide::kLeft, asc::DenseBlasTranspose::kNone,
          f.View(4, 3), Vector(tau), q.View(4, 3), report);
    } else {
      return asc::QueryOrmqrWorkspace(
          provider, asc::DenseBlasSide::kLeft, asc::DenseBlasTranspose::kNone,
          f.View(4, 3), Vector(tau), q.View(4, 3), report);
    }
  };
  const auto plan = Take(query());
  Scratch<T> scratch(plan, false);
  const auto status = [&] {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::Unmqr(provider, asc::DenseBlasSide::kLeft,
                        asc::DenseBlasTranspose::kNone, f.View(4, 3),
                        Vector(tau), q.View(4, 3), plan, scratch.workspace,
                        report);
    } else {
      return asc::Ormqr(provider, asc::DenseBlasSide::kLeft,
                        asc::DenseBlasTranspose::kNone, f.View(4, 3),
                        Vector(tau), q.View(4, 3), plan, scratch.workspace,
                        report);
    }
  }();
  return status.ok() && report.called_provider && report.native_info == 0;
}
template <typename T>
struct Problem {
  Matrix<T> f, x, y, z, b, w, s;
  std::array<std::complex<Real<T>>, 2> eigen{};
  std::array<Real<T>, 2> singular{};
  std::array<Real<T>, 2> residual{};
  std::array<T, 3> tau{};
  explicit Problem(asc::DenseBlasLayout layout)
      : f(layout),
        x(layout == kColumn ? kRow : kColumn),
        y(layout),
        z(layout == kColumn ? kRow : kColumn),
        b(layout),
        w(layout == kColumn ? kRow : kColumn),
        s(layout) {}
  auto Buffers() {
    return asc::LapackDmdQrBuffers<T>{
        f.View(4, 3),     x.View(3, 2),     y.View(3, 3), z.View(4, 2),
        b.View(3, 2),     w.View(2, 2),     s.View(2, 2), Vector(eigen),
        Vector(singular), Vector(residual), Vector(tau)};
  }
  void Initialize() {
    for (int j = 0; j < 3; ++j) {
      for (int i = 0; i < 4; ++i) {
        T value = i < 2 ? T{1} : T{};
        for (int h = 0; h < j; ++h) {
          value *= Eigenvalue<T>(i);
        }
        f.At(i, j) = value;
      }
    }
  }
};
template <typename T>
bool Check(const Problem<T>& p, const Matrix<T>& original, const Matrix<T>& q,
           asc::LapackDmdQrVectors vectors) {
  const auto tolerance = Real<T>{512} * std::numeric_limits<Real<T>>::epsilon();
  const auto a = Eigenvalue<T>(0);
  const auto b = Eigenvalue<T>(1);
  if (std::abs(p.eigen[0] + p.eigen[1] - a - b) > tolerance ||
      std::abs(p.eigen[0] * p.eigen[1] - a * b) > tolerance) {
    return false;
  }
  for (int j = 0; j < 2; ++j) {
    Real<T> norm{};
    for (int i = 0; i < 4; ++i) {
      T mode = p.z.At(i, j);
      if (vectors == asc::LapackDmdQrVectors::kPodFactored) {
        mode = p.z.At(i, 0) * p.w.At(0, j) + p.z.At(i, 1) * p.w.At(1, j);
      }
      const std::complex<Real<T>> wide = mode;
      norm += std::norm(wide);
      const T diagonal = i < 2 ? Eigenvalue<T>(i) : T{};
      T extra{};
      T pod{};
      for (int h = 0; h < 3; ++h) {
        extra += q.At(i, h) * p.b.At(h, j);
        pod += q.At(i, h) * p.x.At(h, j);
      }
      if (std::abs((diagonal - p.eigen[j]) * wide) > tolerance ||
          std::abs(extra - diagonal * pod) > tolerance) {
        return false;
      }
    }
    if (std::abs(norm - Real<T>{1}) > tolerance ||
        (vectors == asc::LapackDmdQrVectors::kExplicit &&
         std::abs(p.residual[j]) > tolerance)) {
      return false;
    }
  }
  Real<T> original_norm{};
  Real<T> singular_norm{};
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 4; ++i) {
      T restored{};
      for (int h = 0; h < 3; ++h) {
        restored += q.At(i, h) * p.y.At(h, j);
      }
      if (std::abs(restored - original.At(i, j)) >
          tolerance * std::max(Real<T>{1}, std::abs(original.At(i, j)))) {
        return false;
      }
      if (j < 2) {
        original_norm += std::norm(std::complex<Real<T>>(original.At(i, j)));
      }
    }
  }
  for (const auto singular : p.singular) {
    singular_norm += singular * singular;
  }
  return std::abs(singular_norm - original_norm) < tolerance * original_norm;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, asc::LapackDmdSvd svd,
         asc::LapackDmdQrVectors vectors, asc::DenseBlasLayout layout,
         bool preferred, bool explicit_q) {
  Problem<T> p(layout);
  const asc::LapackDmdQrOptions options{
      asc::LapackDmdScaling::kNone,
      vectors,
      asc::LapackDmdExtra::kRefinement,
      svd,
      vectors == asc::LapackDmdQrVectors::kExplicit,
      explicit_q,
      true,
      -1};
  const auto plan = Take(
      asc::QueryGedmdqWorkspace(provider, options, p.Buffers(), Real<T>{0}));
  Scratch<T> scratch(plan, preferred);
  for (int reuse = 0; reuse < 2; ++reuse) {
    p.Initialize();
    const auto original = p.f;
    asc::index_t rank = -1;
    asc::LapackReport report;
    if (!asc::Gedmdq(provider, options, p.Buffers(), Real<T>{0}, rank, plan,
                     scratch.workspace, report)
             .ok() ||
        !report.called_provider || report.native_info != 0 || rank != 2 ||
        report.output_validity != asc::LapackOutputValidity::kComplete) {
      return false;
    }
    const auto factors = p.f.values;
    const auto tau = p.tau;
    Matrix<T> q(layout == kColumn ? kRow : kColumn);
    if (!FormQ(provider, p.f, p.tau, q, explicit_q) || p.f.values != factors ||
        p.tau != tau || !Check(p, original, q, vectors)) {
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
    for (auto vectors : {asc::LapackDmdQrVectors::kExplicit,
                         asc::LapackDmdQrVectors::kPodFactored}) {
      for (auto layout : {kColumn, kRow}) {
        for (bool preferred : {false, true}) {
          for (bool explicit_q : {false, true}) {
            passed = Run<float>(provider, svd, vectors, layout, preferred,
                                explicit_q) &&
                     passed;
            passed = Run<double>(provider, svd, vectors, layout, preferred,
                                 explicit_q) &&
                     passed;
            passed = Run<std::complex<float>>(provider, svd, vectors, layout,
                                              preferred, explicit_q) &&
                     passed;
            passed = Run<std::complex<double>>(provider, svd, vectors, layout,
                                               preferred, explicit_q) &&
                     passed;
          }
        }
      }
    }
  }
  g_returned = true;
  std::puts(passed ? "Public GEDMDQ trajectory modes, diagnostics, QR factor "
                     "reuse and plan reuse passed"
                   : "GEDMDQ check failed");
  return passed ? 0 : 1;
}
