#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "asc/dense/providers/lapack_mixed_general.h"
#include "asc/dense/providers/lapack_mixed_positive.h"

namespace {
// Detect a provider STOP that exits(0) before this example returns.
bool g_returned = false;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::abort();
  }
  return std::move(*result);
}
template <typename T>
T Value(double real, double imaginary = 0) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {real, imaginary};
  } else {
    return real;
  }
}
template <typename T>
auto Matrix(std::array<T, 4>& data, asc::DenseBlasLayout layout) {
  return Take(asc::DenseBlasMatrixView<T>::Create(
      data.data(), 2, 2, layout, 2, {data.data(), sizeof(data), kHost}));
}
template <typename T>
struct Scratch {
  using Low =
      std::conditional_t<asc::DenseBlasComplex<T>, std::complex<float>, float>;
  std::array<T, 4> scalar{};
  std::array<T, 16> layout{};
  std::array<Low, 8> lower{};
  std::array<double, 2> real{};
  alignas(16) std::array<std::byte, 32> integer{};
  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto set = [&](asc::LapackWorkspaceKind kind, auto& data) {
      const auto role = static_cast<std::size_t>(kind);
      const auto bytes =
          static_cast<std::size_t>(plan.regions[role].minimum_entries) *
          plan.regions[role].entry_bytes;
      if (bytes > sizeof(data)) {
        std::abort();
      }
      if (bytes != 0) {
        workspace.regions[role] = {data.data(), bytes, kHost};
      }
    };
    set(asc::LapackWorkspaceKind::kScalar, scalar);
    set(asc::LapackWorkspaceKind::kLayoutConversion, layout);
    set(asc::LapackWorkspaceKind::kScratch, lower);
    set(asc::LapackWorkspaceKind::kReal, real);
    set(asc::LapackWorkspaceKind::kInteger, integer);
    return workspace;
  }
};
std::size_t Offset(asc::DenseBlasLayout layout, std::size_t i, std::size_t j) {
  return layout == kColumn ? j * 2 + i : i * 2 + j;
}
template <typename T>
T Exact(std::size_t i, std::size_t j) {
  return Value<T>(1 + static_cast<double>(i) / 4 + static_cast<double>(j),
                  0.25 + static_cast<double>(j) / 8);
}
template <typename T>
bool Reuse(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasMatrixView<T> av, asc::DenseBlasTriangle uplo,
           std::array<T, 4>& b, asc::DenseBlasLayout bl,
           asc::LapackReport& report, Scratch<T>& scratch) {
  // Working-precision POTRF factors are reusable with their selected triangle.
  const auto factor =
      Take(asc::LapackCholeskyFactorView<T>::Create(av, uplo, report));
  for (auto& value : b) {
    value *= 2;
  }
  const auto rhs = Matrix(b, bl);
  const auto reuse = Take(asc::QueryPotrsWorkspace(provider, factor, rhs));
  const auto solved = asc::Potrs(provider, factor, rhs, reuse,
                                 scratch.Workspace(reuse), report);
  if (!solved.ok() || report.native_info != 0) {
    return false;
  }
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      if (std::abs(b[Offset(bl, i, j)] - 2.0 * Exact<T>(i, j)) > 0x1p-44) {
        return false;
      }
    }
  }
  return true;
}
template <typename T>
bool Solve(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasLayout al, asc::DenseBlasLayout bl,
           asc::DenseBlasLayout xl, bool fallback,
           asc::DenseBlasTriangle uplo) {
  const double scale = fallback ? 0x1p150 : 1;
  const std::array<T, 4> original{
      Value<T>(4) * scale, Value<T>(0.25, -0.125) * scale,
      Value<T>(0.25, 0.125) * scale, Value<T>(5) * scale};
  std::array<T, 4> a{};
  std::array<T, 4> b{};
  std::array<T, 4> x{};
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      a[Offset(al, i, j)] = original[i * 2 + j];
      b[Offset(bl, i, j)] = original[i * 2] * Exact<T>(0, j) +
                            original[i * 2 + 1] * Exact<T>(1, j);
    }
  }
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      if (i != j && ((uplo == asc::DenseBlasTriangle::kUpper) == (i > j))) {
        a[Offset(al, i, j)] = Value<T>(991, -997);
      }
    }
  }
  const auto before_a = a;
  const auto before_b = b;
  const auto av = Matrix(a, al);
  const asc::DenseBlasMatrixView<const T> bv = Matrix(b, bl);
  const auto xv = Matrix(x, xl);
  const auto plan = Take([&] {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::QueryZcposvWorkspace(provider, av, uplo, bv, xv);
    } else {
      return asc::QueryDsposvWorkspace(provider, av, uplo, bv, xv);
    }
  }());
  Scratch<T> scratch;
  asc::LapackMixedSolveStatistics statistics;
  asc::LapackReport report;
  const auto status = [&] {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::Zcposv(provider, av, uplo, bv, xv, plan,
                         scratch.Workspace(plan), statistics, report);
    } else {
      return asc::Dsposv(provider, av, uplo, bv, xv, plan,
                         scratch.Workspace(plan), statistics, report);
    }
  }();
  if (!status.ok() || !report.called_provider || report.native_info != 0 ||
      !statistics.native_iteration || b != before_b ||
      (fallback &&
       statistics.fallback != asc::LapackMixedFallback::kConversionRange) ||
      (!fallback && (statistics.fallback != asc::LapackMixedFallback::kNone ||
                     a != before_a))) {
    return false;
  }
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      if (std::abs(x[Offset(xl, i, j)] - Exact<T>(i, j)) > 0x1p-45) {
        return false;
      }
    }
  }
  if (fallback && !Reuse(provider, av, uplo, b, bl, report, scratch)) {
    return false;
  }
  std::printf(
      "scalar_bytes=%zu fallback=%d ITER=%lld: solve and applicable factor "
      "reuse passed\n",
      sizeof(T), static_cast<int>(fallback),
      static_cast<long long>(statistics.native_iteration.value_or(-999)));
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
  for (const auto al : {kColumn, kRow}) {
    for (const auto bl : {kColumn, kRow}) {
      for (const auto xl : {kColumn, kRow}) {
        for (const auto uplo :
             {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
          for (const bool fallback : {false, true}) {
            passed =
                Solve<double>(provider, al, bl, xl, fallback, uplo) && passed;
            passed = Solve<std::complex<double>>(provider, al, bl, xl, fallback,
                                                 uplo) &&
                     passed;
          }
        }
      }
    }
  }
  g_returned = true;
  return passed ? 0 : 1;
}
