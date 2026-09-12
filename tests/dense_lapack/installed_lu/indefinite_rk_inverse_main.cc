#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk.h"
#include "asc/dense/providers/lapack_indefinite_rk_inverse.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
template <typename T>
struct Scratch {
  std::array<T, 17000> scalar{};
  std::array<T, 5000> packing{};
  alignas(std::max_align_t) std::array<std::byte, 1024> integers{};
  asc::LapackWorkspace workspace;

  Scratch() {
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kScalar)] = {scalar.data(), sizeof(scalar),
                                               installed_internal::kHost};
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kLayoutConversion)] = {
        packing.data(), sizeof(packing), installed_internal::kHost};
    workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kInteger)] = {
        integers.data(), integers.size(), installed_internal::kHost};
  }
};

asc::extent_t g_block_size = 0;
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> factors,
           asc::DenseBlasVectorView<const T> extra,
           asc::RawLapackPivotView pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return g_block_size == 0
                 ? asc::QueryHetri3Workspace(provider, triangle, factors, extra,
                                             pivots)
                 : asc::QueryHetri3xWorkspace(provider, triangle, g_block_size,
                                              factors, extra, pivots);
    }
  }
  return g_block_size == 0
             ? asc::QuerySytri3Workspace(provider, triangle, factors, extra,
                                         pivots)
             : asc::QuerySytri3xWorkspace(provider, triangle, g_block_size,
                                          factors, extra, pivots);
}
template <typename T>
asc::Status Inverse(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    asc::DenseBlasMatrixView<T> factors,
                    asc::DenseBlasVectorView<const T> extra,
                    asc::RawLapackPivotView pivots,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return g_block_size == 0
                 ? asc::Hetri3(provider, triangle, factors, extra, pivots, plan,
                               workspace, report)
                 : asc::Hetri3x(provider, triangle, g_block_size, factors,
                                extra, pivots, plan, workspace, report);
    }
  }
  return g_block_size == 0
             ? asc::Sytri3(provider, triangle, factors, extra, pivots, plan,
                           workspace, report)
             : asc::Sytri3x(provider, triangle, g_block_size, factors, extra,
                            pivots, plan, workspace, report);
}
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool hermitian, bool blocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<T> extra,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::QueryHetrfRkWorkspace(provider, triangle, matrix,
                                                  extra, pivots)
                     : asc::QueryHetf2RkWorkspace(provider, triangle, matrix,
                                                  extra, pivots);
    }
  }
  return blocked ? asc::QuerySytrfRkWorkspace(provider, triangle, matrix, extra,
                                              pivots)
                 : asc::QuerySytf2RkWorkspace(provider, triangle, matrix, extra,
                                              pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   bool blocked, asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> extra,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::HetrfRk(provider, triangle, matrix, extra, pivots,
                                    plan, workspace, report)
                     : asc::Hetf2Rk(provider, triangle, matrix, extra, pivots,
                                    plan, workspace, report);
    }
  }
  return blocked ? asc::SytrfRk(provider, triangle, matrix, extra, pivots, plan,
                                workspace, report)
                 : asc::Sytf2Rk(provider, triangle, matrix, extra, pivots, plan,
                                workspace, report);
}

template <typename T, std::size_t N>
asc::Status MakeFactor(const asc::ReferenceLapackProvider& provider,
                       bool hermitian, asc::DenseBlasTriangle triangle,
                       int origin, installed_internal::Matrix<T, N, N>& a,
                       asc::DenseBlasVectorView<T> extra,
                       asc::DenseBlasVectorView<asc::index_t> pivots,
                       asc::LapackReport& report) {
  Scratch<T> scratch;
  const auto plan = Take(QueryFactor(provider, triangle, hermitian, origin == 0,
                                     a.View(), extra, pivots));
  return Factor(provider, triangle, hermitian, origin == 0, a.View(), extra,
                pivots, plan, scratch.workspace, report);
}

template <typename T, std::size_t N>
T Coefficient(std::size_t i, std::size_t j, bool hermitian, bool singular) {
  if constexpr (N == 1) {
    return singular ? T{} : T{4};
  }
  if (i + j == 1) {
    return installed_internal::Value<T>(3, hermitian && i == 0 ? -4 : 4);
  }
  if (i == j && i >= 2 && (!singular || i != N - 1)) {
    return installed_internal::Value<T>(i % 2 == 0 ? 8 : -8, hermitian ? 0 : 6);
  }
  return T{};
}

template <typename T, std::size_t N>
bool Output(const installed_internal::Matrix<T, N, N>& a,
            const installed_internal::Matrix<T, N, N>& before, bool hermitian,
            asc::DenseBlasTriangle triangle, bool singular, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  bool ok = a.PaddingEquals(before.data);
  if (singular) {
    return ok && std::memcmp(static_cast<const void*>(a.data.data()),
                             static_cast<const void*>(before.data.data()),
                             sizeof(a.data)) == 0;
  }
  const Real scale = std::ldexp(Real{1}, exponent);
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      const bool selected =
          triangle == asc::DenseBlasTriangle::kUpper ? i <= j : i >= j;
      if (!selected) {
        ok = ok && a.At(i, j) == before.At(i, j);
        continue;
      }
      installed_internal::Wide expected{};
      if constexpr (N == 1) {
        expected = 0.25L;
      } else if (i + j == 1 || (i == j && i >= 2)) {
        expected = installed_internal::Wide{1} /
                   installed_internal::Widen(
                       Coefficient<T, N>(j, i, hermitian, false));
      }
      expected /= scale;
      const auto value = installed_internal::Widen(a.At(i, j));
      if (hermitian && i == j) {
        ok = value.imag() == 0 && ok;
      }
      ok = ok && std::isfinite(value.real()) && std::isfinite(value.imag()) &&
           std::abs(value - expected) <=
               64 * std::numeric_limits<Real>::epsilon() * std::abs(expected);
    }
  }
  return ok;
}

template <typename T, std::size_t N>
bool Check(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           int origin, bool singular, int exponent = 0) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  a.data.fill(T{-37});
  using Real = asc::DenseBlasRealType<T>;
  const Real scale = std::ldexp(Real{1}, exponent);
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      if (triangle == asc::DenseBlasTriangle::kUpper ? i <= j : i >= j) {
        a.At(i, j) = scale * Coefficient<T, N>(i, j, hermitian, singular);
      }
    }
  }
  std::array<asc::index_t, N + 2> p;
  p.fill(-41);
  const auto pivot_output = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      p.data() + 1, N, 1, {p.data(), sizeof(p), installed_internal::kHost}));
  std::array<T, N + 2> e;
  e.fill(installed_internal::Value<T>(-43, 17));
  const auto extra_output = Take(asc::DenseBlasVectorView<T>::Create(
      e.data() + 1, N, 1, {e.data(), sizeof(e), installed_internal::kHost}));
  asc::LapackReport factor_report;
  const auto factor_status =
      MakeFactor(provider, hermitian, triangle, origin, a, extra_output,
                 pivot_output, factor_report);
  bool ok = factor_status.ok() != singular;
  for (std::size_t i = 0; i < N;) {
    const bool pair = p[i + 1] < 0;
    const auto ignored =
        pair && triangle == asc::DenseBlasTriangle::kLower ? i + 1 : i;
    e[ignored + 1] = installed_internal::Value<T>(
        std::numeric_limits<Real>::quiet_NaN(), 19);
    i += pair ? 2 : 1;
  }
  const auto old_e = e;
  const auto extra = asc::DenseBlasVectorView<const T>(extra_output);
  const auto before = a;
  const auto old_p = p;
  const auto raw = Take(asc::RawLapackPivotView::Create(
      p.data() + 1, N, asc::LapackFactorFamily::kRook,
      {p.data(), sizeof(p), installed_internal::kHost}));
  const auto plan =
      Take(Query(provider, triangle, hermitian, a.View(), extra, raw));
  Scratch<T> scratch;
  asc::LapackReport report;
  const auto status = Inverse(provider, triangle, hermitian, a.View(), extra,
                              raw, plan, scratch.workspace, report);
  ok = ok && status.ok() != singular && report.called_provider == (N != 0) &&
       p == old_p;
  if constexpr (N == 0) {
    ok = ok && !report.native_info.has_value();
  } else {
    ok = ok &&
         report.native_info == (singular ? static_cast<asc::index_t>(N) : 0);
  }
  if (singular) {
    ok =
        ok && report.outcome == asc::LapackOutcome::kSingular &&
        report.output_validity == asc::LapackOutputValidity::kDocumentedPartial;
  } else {
    ok = ok && report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete;
  }
  ok = Output(a, before, hermitian, triangle, singular, exponent) && ok;
  ok = report.factor_family == asc::LapackFactorFamily::kRook && ok;
  ok = std::memcmp(static_cast<const void*>(e.data()),
                   static_cast<const void*>(old_e.data()), sizeof(e)) == 0 &&
       ok;
  if (!ok) {
    std::fprintf(stderr,
                 "installed RK inverse failure N=%zu he=%d tri=%d layout=%d "
                 "origin=%d singular=%d\n",
                 N, static_cast<int>(hermitian), static_cast<int>(triangle),
                 static_cast<int>(layout), origin, static_cast<int>(singular));
  }
  return ok;
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool hermitian,
         int& cases) {
  bool ok = true;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto layout :
         {installed_internal::kColumn, installed_internal::kRow}) {
      for (const int origin : {0, 1}) {
        ok =
            Check<T, 0>(provider, hermitian, triangle, layout, origin, false) &&
            ok;
        ok =
            Check<T, 1>(provider, hermitian, triangle, layout, origin, false) &&
            ok;
        ok = Check<T, 1>(provider, hermitian, triangle, layout, origin, true) &&
             ok;
        ok =
            Check<T, 2>(provider, hermitian, triangle, layout, origin, false) &&
            ok;
        ok =
            Check<T, 7>(provider, hermitian, triangle, layout, origin, false) &&
            ok;
        ok = Check<T, 7>(provider, hermitian, triangle, layout, origin, true) &&
             ok;
        ok = Check<T, 67>(provider, hermitian, triangle, layout, origin,
                          false) &&
             ok;
        ok =
            Check<T, 67>(provider, hermitian, triangle, layout, origin, true) &&
            ok;
        ok =
            Check<T, 3>(provider, hermitian, triangle, layout, origin, false) &&
            ok;
        ok = Check<T, 3>(provider, hermitian, triangle, layout, origin, true) &&
             ok;
        const int exponent = sizeof(asc::DenseBlasRealType<T>) == 4 ? 100 : 800;
        for (const int scale : {-exponent, exponent}) {
          ok = Check<T, 7>(provider, hermitian, triangle, layout, origin, false,
                           scale) &&
               ok;
          ok = Check<T, 67>(provider, hermitian, triangle, layout, origin,
                            false, scale) &&
               ok;
        }
        cases += 14;
      }
    }
  }
  return ok;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  bool ok = true;
  for (const asc::extent_t block : {0, 1, 2, 3, 64}) {
    g_block_size = block;
    ok = Run<float>(provider, false, cases) && ok;
    ok = Run<double>(provider, false, cases) && ok;
    ok = Run<std::complex<float>>(provider, false, cases) && ok;
    ok = Run<std::complex<double>>(provider, false, cases) && ok;
    ok = Run<std::complex<float>>(provider, true, cases) && ok;
    ok = Run<std::complex<double>>(provider, true, cases) && ok;
  }
  std::printf("installed RK inverse cases=%d passed=%d\n", cases,
              static_cast<int>(ok));
  return ok ? 0 : 1;
}
