#include <array>
#include <cmath>
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
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
#include "asc/dense/providers/lapack_positive_tridiagonal_condition.h"

namespace {
constexpr auto kHost = asc::MemorySpace::kHost;
template <typename T>
T Take(asc::Result<T> value) {
  if (!value.ok()) {
    std::abort();
  }
  return std::move(*value);
}
template <typename T>
T Value(double real, double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}
template <typename T>
T Conjugate(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::conj(value);
  } else {
    return value;
  }
}
template <typename T, std::size_t N>
auto Vector(std::array<T, N>& array) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      array.data(), N, 1, {array.data(), sizeof(array), kHost}));
}
template <typename T>
bool SolveTwice(const asc::ReferenceLapackProvider& provider,
                asc::ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
                const std::array<T, 4>& matrix, asc::DenseBlasLayout layout) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 8> b{};
  std::array<T, 4> packing{};
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      b.data(), 2, 2, layout, 4, {b.data(), sizeof(b), kHost}));
  const auto solve_plan = Take(asc::QueryPttrsWorkspace(provider, factor, rhs));
  asc::LapackWorkspace solve_workspace;
  constexpr auto kPacking =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
  if (solve_plan.regions[kPacking].minimum_entries != 0) {
    solve_workspace.regions[kPacking] = {packing.data(), sizeof(packing),
                                         kHost};
  }
  const auto at = [&](int i, int j) -> T& {
    return b[static_cast<std::size_t>(
        layout == asc::DenseBlasLayout::kRowMajor ? 4 * i + j : 4 * j + i)];
  };
  for (int pass = 0; pass < 2; ++pass) {
    const auto exact = [pass](int i, int j) {
      return Value<T>(1 + i + 2 * j + pass, 0.5 + i - j);
    };
    b.fill(Value<T>(-77));
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        at(i, j) = T{};
        for (int k = 0; k < 2; ++k) {
          at(i, j) += matrix[2 * static_cast<std::size_t>(i) +
                             static_cast<std::size_t>(k)] *
                      exact(k, j);
        }
      }
    }
    asc::LapackReport report;
    if (!asc::Pttrs(provider, factor, rhs, solve_plan, solve_workspace, report)
             .ok() ||
        report.native_info != 0) {
      return false;
    }
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        if (std::abs(at(i, j) - exact(i, j)) >
            128 * std::numeric_limits<Real>::epsilon() *
                std::abs(exact(i, j))) {
          return false;
        }
      }
    }
    for (const auto i : {2U, 3U, 6U, 7U}) {
      if (b[i] != Value<T>(-77)) {
        return false;
      }
    }
  }
  return true;
}
template <typename T>
bool Reuse(const asc::ReferenceLapackProvider& provider,
           asc::ReferencePositiveDefiniteTridiagonalFactorView<T> factor,
           const std::array<T, 4>& matrix, asc::DenseBlasRealType<T> norm,
           long double expected) {
  using Real = asc::DenseBlasRealType<T>;
  Real output = -19;
  std::array<Real, 2> work{};
  const auto plan =
      Take(asc::QueryPtconWorkspace(provider, factor, norm, output));
  asc::LapackWorkspace workspace;
  constexpr auto kReal =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
  if (plan.regions[kReal].minimum_entries != 2) {
    return false;
  }
  workspace.regions[kReal] = {work.data(), sizeof(work), kHost};
  for (const auto layout :
       {asc::DenseBlasLayout::kRowMajor, asc::DenseBlasLayout::kColumnMajor}) {
    if (!SolveTwice(provider, factor, matrix, layout)) {
      return false;
    }
    for (int pass = 0; pass < 2; ++pass) {
      asc::LapackReport report;
      if (!asc::Ptcon(provider, factor, norm, output, plan, workspace, report)
               .ok() ||
          !report.called_provider || report.native_info != 0 ||
          report.factor_family.has_value() ||
          report.output_validity != asc::LapackOutputValidity::kComplete) {
        return false;
      }
      if (std::abs(output - expected) >
          128 * std::numeric_limits<Real>::epsilon() * expected) {
        return false;
      }
    }
  }
  return true;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  // Ordinary independently constructed 2x2 Hermitian matrix. Its inverse is
  // [d1,-conj(e);-e,d0]/det, so the two one-norms are norm and norm/det.
  std::array<Real, 2> d{4, 5};
  std::array<T, 1> e{Value<T>(0.5, 0.25)};
  const std::array<T, 4> original{T{4}, Conjugate(e[0]), e[0], T{5}};
  const long double magnitude = static_cast<long double>(std::abs(e[0]));
  const Real norm = static_cast<Real>(5 + magnitude);
  const long double expected =
      (20 - magnitude * magnitude) / ((5 + magnitude) * (5 + magnitude));
  const auto matrix =
      Take(asc::LapackPositiveDefiniteTridiagonalView<T>::Create(Vector(d),
                                                                 Vector(e)));
  const auto plan = Take(asc::QueryPttrfWorkspace(provider, matrix));
  asc::LapackReport report;
  if (!asc::Pttrf(provider, matrix, plan, {}, report).ok() ||
      report.native_info != 0) {
    return false;
  }
  const auto immutable =
      Take(asc::LapackPositiveDefiniteTridiagonalView<const T>::Create(
          Vector(d), Vector(e)));
  const auto lower =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::Create(
          provider, immutable, report));
  if (!Reuse(provider, lower, original, norm, expected)) {
    return false;
  }
  asc::HostMemoryResource resource;
  auto upper =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactor<T>::CopyFrom(
          provider, lower, asc::DenseBlasTriangle::kUpper, resource));
  d.fill(-1);
  e.fill(Value<T>(99, -99));
  if (!Reuse(provider, Take(upper.view(provider)), original, norm, expected)) {
    return false;
  }
  std::printf(
      "PTCON scalar_bytes=%zu: independent scalar diagnostics, borrowed "
      "lower/owned upper factor reuse, both padded solve layouts\n",
      sizeof(T));
  return true;
}
}  // namespace
int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  return Run<float>(provider) && Run<double>(provider) &&
                 Run<std::complex<float>>(provider) &&
                 Run<std::complex<double>>(provider)
             ? 0
             : 1;
}
