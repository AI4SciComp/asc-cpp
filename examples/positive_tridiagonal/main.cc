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
#include "normal_return_guard.h"

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
                const std::array<T, 9>& a) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto layout :
       {asc::DenseBlasLayout::kRowMajor, asc::DenseBlasLayout::kColumnMajor}) {
    std::array<T, 12> b{};
    std::array<T, 6> packing{};
    const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
        b.data(), 3, 2, layout, 4, {b.data(), sizeof(b), kHost}));
    const auto at = [&](int i, int j) -> T& {
      return b[static_cast<std::size_t>(
          layout == asc::DenseBlasLayout::kRowMajor ? 4 * i + j : 4 * j + i)];
    };
    const auto plan = Take(asc::QueryPttrsWorkspace(provider, factor, rhs));
    asc::LapackWorkspace workspace;
    constexpr auto kPacking =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
    if (plan.regions[kPacking].minimum_entries != 0) {
      if (plan.regions[kPacking].minimum_entries != 6) {
        return false;
      }
      workspace.regions[kPacking] = {packing.data(), sizeof(packing), kHost};
    }
    for (int pass = 0; pass < 2; ++pass) {
      const auto exact = [pass](int i, int j) {
        return Value<T>(1 + i + 2 * j + pass, 0.5 + i - j - pass);
      };
      b.fill(Value<T>(-77));
      for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 3; ++i) {
          at(i, j) = T{};
          for (int k = 0; k < 3; ++k) {
            at(i, j) += a[3 * static_cast<std::size_t>(i) +
                          static_cast<std::size_t>(k)] *
                        exact(k, j);
          }
        }
      }
      asc::LapackReport report;
      if (!asc::Pttrs(provider, factor, rhs, plan, workspace, report).ok() ||
          !report.called_provider || report.native_info != 0 ||
          report.output_validity != asc::LapackOutputValidity::kComplete) {
        return false;
      }
      for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 3; ++i) {
          if (std::abs(at(i, j) - exact(i, j)) >
              128 * std::numeric_limits<Real>::epsilon() *
                  std::abs(exact(i, j))) {
            return false;
          }
        }
      }
    }
  }
  return true;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  // Independent ordinary Hermitian tridiagonal, strictly diagonally dominant.
  std::array<Real, 3> d{4, 5, 6};
  std::array<T, 2> e{Value<T>(0.5, 0.25), Value<T>(-0.25, 0.5)};
  const std::array<T, 9> original{
      T{4}, Conjugate(e[0]), T{}, e[0], T{5}, Conjugate(e[1]), T{}, e[1], T{6}};
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
  const auto factor =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactorView<T>::Create(
          provider, immutable, report));
  if (!SolveTwice(provider, factor, original)) {
    return false;
  }
  asc::HostMemoryResource resource;
  auto upper =
      Take(asc::ReferencePositiveDefiniteTridiagonalFactor<T>::CopyFrom(
          provider, factor, asc::DenseBlasTriangle::kUpper, resource));
  // Owned factors survive changes to original storage. Conversion conjugates E.
  d.fill(-1);
  e.fill(Value<T>(99, -99));
  if (!SolveTwice(provider, Take(upper.view(provider)), original)) {
    return false;
  }
  std::printf(
      "installed PT real_bytes=%zu scalar_bytes=%zu: factor once, borrowed "
      "lower and owned upper, solve twice, both RHS layouts\n",
      sizeof(Real), sizeof(T));
  return true;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  return Run<float>(provider) && Run<double>(provider) &&
                 Run<std::complex<float>>(provider) &&
                 Run<std::complex<double>>(provider)
             ? 0
             : 1;
}
