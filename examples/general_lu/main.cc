#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
using Wide = std::complex<long double>;
bool g_returned = false;
bool Finite(Wide value) {
  return std::isfinite(value.real()) && std::isfinite(value.imag());
}
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}
template <typename T>
T Narrow(Wide value) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(value.real()), static_cast<Real>(value.imag())};
  } else {
    return static_cast<Real>(value.real());
  }
}
template <typename T>
Wide Widen(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}
template <typename T, std::size_t Columns>
struct Matrix {
  std::array<T, 32> values{};
  asc::DenseBlasLayout layout;
  explicit Matrix(asc::DenseBlasLayout order) : layout(order) {
    values.fill(Narrow<T>({-79, 11}));
  }
  T& At(std::size_t i, std::size_t j) {
    return values[1 + (layout == kColumn ? 5 * j + i : 5 * i + j)];
  }
  [[nodiscard]] const T& At(std::size_t i, std::size_t j) const {
    return values[1 + (layout == kColumn ? 5 * j + i : 5 * i + j)];
  }
  auto View() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 1, 3, Columns, layout, 5,
        {values.data(), sizeof(values), kHost}));
  }
  [[nodiscard]] bool Guards() const {
    auto expected = values;
    for (std::size_t i = 0; i < 3; ++i) {
      for (std::size_t j = 0; j < Columns; ++j) {
        expected[1 + (layout == kColumn ? 5 * j + i : 5 * i + j)] =
            Narrow<T>({-79, 11});
      }
    }
    return std::all_of(expected.begin(), expected.end(),
                       [](T value) { return value == Narrow<T>({-79, 11}); });
  }
};
template <typename T>
struct Scratch {
  std::array<T, 1024> scalar{};
  std::array<T, 32> layout{};
  alignas(16) std::array<std::byte, 64> integer{};
  asc::LapackWorkspace All() {
    asc::LapackWorkspace workspace;
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kScalar)] = {scalar.data(), sizeof(scalar),
                                               kHost};
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kLayoutConversion)] = {layout.data(),
                                                         sizeof(layout), kHost};
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kInteger)] = {integer.data(), integer.size(),
                                                kHost};
    return workspace;
  }
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    auto workspace = All();
    for (std::size_t role = 0; role < plan.regions.size(); ++role) {
      const auto& need = plan.regions[role];
      const auto bytes =
          static_cast<std::size_t>(need.preferred_entries) * need.entry_bytes;
      if (bytes > workspace.regions[role].size()) {
        std::abort();
      }
      const auto data = workspace.regions[role].data();
      workspace.regions[role] = {bytes == 0 ? nullptr : data, bytes, kHost};
    }
    return workspace;
  }
};
bool Successful(const asc::Status& status, const asc::LapackReport& report) {
  return status.ok() && report.called_provider && report.native_info == 0 &&
         report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete;
}
template <typename T>
void Initialize(Matrix<T, 3>& a) {
  const std::array<Wide, 9> fixture{{{0, 0},
                                     {2, 0.5},
                                     {1, -0.25},
                                     {4, -0.5},
                                     {6, 0.125},
                                     {-0.25, 0.125},
                                     {1, 0.25},
                                     {-0.5, -0.125},
                                     {8, -0.25}}};
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      a.At(i, j) = Narrow<T>(fixture[3 * i + j]);
    }
  }
}
template <typename T>
Wide Coefficient(const Matrix<T, 3>& a, std::size_t i, std::size_t j,
                 asc::DenseBlasTranspose trans) {
  if (trans == asc::DenseBlasTranspose::kNone) {
    return Widen(a.At(i, j));
  }
  const auto value = Widen(a.At(j, i));
  return trans == asc::DenseBlasTranspose::kTranspose ? value
                                                      : std::conj(value);
}
template <typename T>
Wide Exact(std::size_t i, std::size_t j, int multiplier) {
  return static_cast<long double>(multiplier) *
         Widen(Narrow<T>({1 + i / 8.0L + j / 4.0L, 0.25L + (i + j) / 8.0L}));
}
template <typename T>
void RightHandSide(const Matrix<T, 3>& a, Matrix<T, 2>& b,
                   asc::DenseBlasTranspose trans, int multiplier) {
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      Wide sum{};
      for (std::size_t k = 0; k < 3; ++k) {
        sum += Coefficient(a, i, k, trans) * Exact<T>(k, j, multiplier);
      }
      b.At(i, j) = Narrow<T>(sum);
    }
  }
}
template <typename T>
bool Solution(const Matrix<T, 3>& a, const Matrix<T, 2>& b,
              const Matrix<T, 2>& x, asc::DenseBlasTranspose trans,
              int multiplier) {
  constexpr long double kTolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      const auto value = Widen(x.At(i, j));
      if (!std::isfinite(value.real()) || !std::isfinite(value.imag()) ||
          std::abs(value - Exact<T>(i, j, multiplier)) >
              kTolerance * std::abs(Exact<T>(i, j, multiplier))) {
        return false;
      }
      Wide residual = -Widen(b.At(i, j));
      long double scale = std::abs(residual);
      for (std::size_t k = 0; k < 3; ++k) {
        const auto term = Coefficient(a, i, k, trans) * Widen(x.At(k, j));
        residual += term;
        scale += std::abs(term);
      }
      if (std::abs(residual) > kTolerance * scale) {
        return false;
      }
    }
  }
  return x.Guards();
}
template <typename T>
bool Reconstruct(const Matrix<T, 3>& original, const Matrix<T, 3>& factor,
                 const std::array<asc::index_t, 5>& pivots) {
  std::array<std::size_t, 3> permutation{0, 1, 2};
  for (std::size_t i = 0; i < 3; ++i) {
    const auto pivot = pivots[i + 1];
    if (pivot < static_cast<asc::index_t>(i + 1) || pivot > 3) {
      return false;
    }
    std::swap(permutation[i], permutation[static_cast<std::size_t>(pivot - 1)]);
  }
  long double error = 0;
  long double norm = 0;
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      Wide product{};
      for (std::size_t k = 0; k < 3; ++k) {
        Wide lower{};
        if (i == k) {
          lower = {1, 0};
        } else if (i > k) {
          lower = Widen(factor.At(i, k));
        }
        product += lower * (k <= j ? Widen(factor.At(k, j)) : Wide{});
      }
      if (!Finite(product)) {
        return false;
      }
      error = std::max(
          error, std::abs(product - Widen(original.At(permutation[i], j))));
      norm = std::max(norm, std::abs(Widen(original.At(i, j))));
    }
  }
  return error <=
             128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() *
                 norm &&
         pivots.front() == -19 && pivots.back() == -19 && factor.Guards();
}
template <typename T>
bool Inverse(const asc::ReferenceLapackProvider& provider,
             const Matrix<T, 3>& original, Matrix<T, 3> factor,
             asc::RawLapackPivotView pivots, Scratch<T>& scratch) {
  asc::LapackReport report;
  const auto plan = asc::QueryGetriWorkspace(provider, factor.View(), pivots,
                                             scratch.All(), report);
  if (!plan.ok() || !report.called_provider || report.native_info != 0 ||
      !Successful(asc::Getri(provider, factor.View(), pivots, *plan,
                             scratch.Workspace(*plan), report),
                  report)) {
    return false;
  }
  for (std::size_t i = 0; i < 3; ++i) {
    for (std::size_t j = 0; j < 3; ++j) {
      Wide left{};
      Wide right{};
      for (std::size_t k = 0; k < 3; ++k) {
        left += Widen(original.At(i, k)) * Widen(factor.At(k, j));
        right += Widen(factor.At(i, k)) * Widen(original.At(k, j));
      }
      const Wide expected = i == j ? Wide{1, 0} : Wide{};
      if (!Finite(left) || !Finite(right) ||
          std::abs(left - expected) >
              128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() ||
          std::abs(right - expected) >
              128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon()) {
        return false;
      }
    }
  }
  return factor.Guards();
}
template <typename T>
bool Exercise(const asc::ReferenceLapackProvider& provider, int algorithm,
              asc::DenseBlasLayout al, asc::DenseBlasLayout bl) {
  Matrix<T, 3> a(al);
  Initialize(a);
  const auto original = a;
  std::array<asc::index_t, 5> pivot_values{};
  pivot_values.fill(-19);
  const auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivot_values.data() + 1, 3, 1,
      {pivot_values.data(), sizeof(pivot_values), kHost}));
  const auto plan = Take([&] {
    if (algorithm == 0) {
      return asc::QueryGetrfWorkspace(provider, a.View(), pivots);
    }
    if (algorithm == 1) {
      return asc::QueryGetrf2Workspace(provider, a.View(), pivots);
    }
    return asc::QueryGetf2Workspace(provider, a.View(), pivots);
  }());
  Scratch<T> scratch;
  const auto work = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto status = [&] {
    if (algorithm == 0) {
      return asc::Getrf(provider, a.View(), pivots, plan, work, report);
    }
    if (algorithm == 1) {
      return asc::Getrf2(provider, a.View(), pivots, plan, work, report);
    }
    return asc::Getf2(provider, a.View(), pivots, plan, work, report);
  }();
  if (!Successful(status, report) || !Reconstruct(original, a, pivot_values)) {
    return false;
  }
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivot_values.data() + 1, 3, asc::LapackFactorFamily::kLuPartialPivot,
      {pivot_values.data(), sizeof(pivot_values), kHost}));
  const auto factor =
      Take(asc::LapackLuFactorView<T>::Create(a.View(), raw, report));
  const auto saved = a.values;
  const auto saved_pivots = pivot_values;
  for (const auto trans :
       {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
        asc::DenseBlasTranspose::kConjugateTranspose}) {
    Matrix<T, 2> b(bl);
    RightHandSide(original, b, trans, 1);
    const auto reuse =
        Take(asc::QueryGetrsWorkspace(provider, trans, factor, b.View()));
    for (const int multiplier : {1, 2}) {
      RightHandSide(original, b, trans, multiplier);
      const auto before = b;
      if (!Successful(asc::Getrs(provider, trans, factor, b.View(), reuse,
                                 scratch.Workspace(reuse), report),
                      report) ||
          !Solution(original, before, b, trans, multiplier) ||
          a.values != saved || pivot_values != saved_pivots) {
        return false;
      }
    }
  }
  return Inverse(provider, original, a, raw, scratch);
}
template <typename T>
bool Driver(const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasLayout al, asc::DenseBlasLayout bl) {
  Matrix<T, 3> a(al);
  Initialize(a);
  const auto original = a;
  Matrix<T, 2> b(bl);
  RightHandSide(original, b, asc::DenseBlasTranspose::kNone, 1);
  const auto before = b;
  std::array<asc::index_t, 5> pivots{};
  pivots.fill(-19);
  const auto pv = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, 3, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto plan =
      Take(asc::QueryGesvWorkspace(provider, a.View(), pv, b.View()));
  Scratch<T> scratch;
  asc::LapackReport report;
  return Successful(asc::Gesv(provider, a.View(), pv, b.View(), plan,
                              scratch.Workspace(plan), report),
                    report) &&
         Solution(original, before, b, asc::DenseBlasTranspose::kNone, 1) &&
         Reconstruct(original, a, pivots);
}
template <typename T>
bool Run() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  bool passed = true;
  for (const auto al : {kColumn, kRow}) {
    for (const auto bl : {kColumn, kRow}) {
      for (const int algorithm : {0, 1, 2}) {
        const bool result = Exercise<T>(provider, algorithm, al, bl);
        std::printf(
            "scalar_bytes=%zu algorithm=%d layouts=%d%d N/T/C "
            "reuse/inverse=%d\n",
            sizeof(T), algorithm, static_cast<int>(al), static_cast<int>(bl),
            static_cast<int>(result));
        passed = result && passed;
      }
      passed = Driver<T>(provider, al, bl) && passed;
    }
  }
  return passed;
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
  bool passed = Run<float>();
  passed = Run<double>() && passed;
  passed = Run<std::complex<float>>() && passed;
  passed = Run<std::complex<double>>() && passed;
  std::array<bool, 4> results{};
  std::array<std::thread, 4> threads;
  threads[0] = std::thread([&] { results[0] = Run<float>(); });
  threads[1] = std::thread([&] { results[1] = Run<double>(); });
  threads[2] = std::thread([&] { results[2] = Run<std::complex<float>>(); });
  threads[3] = std::thread([&] { results[3] = Run<std::complex<double>>(); });
  for (auto& thread : threads) {
    thread.join();
  }
  for (const bool result : results) {
    passed = result && passed;
  }
  g_returned = true;
  return passed ? 0 : 1;
}
