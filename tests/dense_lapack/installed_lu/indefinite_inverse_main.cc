#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
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
#include "asc/dense/providers/lapack_indefinite.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
#include "asc/dense/providers/lapack_indefinite_inverse.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> factors,
           asc::RawLapackPivotView pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetriWorkspace(provider, triangle, factors, pivots);
    }
  }
  return asc::QuerySytriWorkspace(provider, triangle, factors, pivots);
}
template <typename T>
asc::Status Inverse(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    asc::DenseBlasMatrixView<T> factors,
                    asc::RawLapackPivotView pivots,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hetri(provider, triangle, factors, pivots, plan, workspace,
                        report);
    }
  }
  return asc::Sytri(provider, triangle, factors, pivots, plan, workspace,
                    report);
}
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool hermitian, bool blocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked
                 ? asc::QueryHetrfWorkspace(provider, triangle, matrix, pivots)
                 : asc::QueryHetf2Workspace(provider, triangle, matrix, pivots);
    }
  }
  return blocked ? asc::QuerySytrfWorkspace(provider, triangle, matrix, pivots)
                 : asc::QuerySytf2Workspace(provider, triangle, matrix, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   bool blocked, asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::Hetrf(provider, triangle, matrix, pivots, plan,
                                  workspace, report)
                     : asc::Hetf2(provider, triangle, matrix, pivots, plan,
                                  workspace, report);
    }
  }
  return blocked ? asc::Sytrf(provider, triangle, matrix, pivots, plan,
                              workspace, report)
                 : asc::Sytf2(provider, triangle, matrix, pivots, plan,
                              workspace, report);
}

template <typename T, std::size_t N>
asc::Status MakeFactor(const asc::ReferenceLapackProvider& provider,
                       bool hermitian, asc::DenseBlasTriangle triangle,
                       int origin, installed_internal::Matrix<T, N, N>& a,
                       asc::DenseBlasVectorView<asc::index_t> pivots,
                       asc::LapackReport& report) {
  installed_internal::Scratch<T> scratch;
  if (origin != 2) {
    const auto plan = Take(QueryFactor(provider, triangle, hermitian,
                                       origin == 0, a.View(), pivots));
    return Factor(provider, triangle, hermitian, origin == 0, a.View(), pivots,
                  plan, scratch.workspace, report);
  }
  installed_internal::Matrix<T, N, 0> rhs{{}, installed_internal::kColumn};
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      const auto plan = Take(asc::QueryHesvWorkspace(
          provider, triangle, a.View(), pivots, rhs.View()));
      return asc::Hesv(provider, triangle, a.View(), pivots, rhs.View(), plan,
                       scratch.workspace, report);
    }
  }
  const auto plan = Take(asc::QuerySysvWorkspace(provider, triangle, a.View(),
                                                 pivots, rhs.View()));
  return asc::Sysv(provider, triangle, a.View(), pivots, rhs.View(), plan,
                   scratch.workspace, report);
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
            asc::DenseBlasTriangle triangle, bool singular) {
  using Real = asc::DenseBlasRealType<T>;
  bool ok = a.PaddingEquals(before.data);
  if (singular) {
    return ok && a.data == before.data;
  }
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
      const auto value = installed_internal::Widen(a.At(i, j));
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
           int origin, bool singular) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  a.data.fill(T{-37});
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      if (triangle == asc::DenseBlasTriangle::kUpper ? i <= j : i >= j) {
        a.At(i, j) = Coefficient<T, N>(i, j, hermitian, singular);
      }
    }
  }
  std::array<asc::index_t, N + 2> p;
  p.fill(-41);
  const auto pivot_output = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      p.data() + 1, N, 1, {p.data(), sizeof(p), installed_internal::kHost}));
  asc::LapackReport factor_report;
  const auto factor_status = MakeFactor(provider, hermitian, triangle, origin,
                                        a, pivot_output, factor_report);
  bool ok = factor_status.ok() != singular;
  const auto before = a;
  const auto old_p = p;
  const auto raw = Take(asc::RawLapackPivotView::Create(
      p.data() + 1, N, asc::LapackFactorFamily::kBunchKaufman,
      {p.data(), sizeof(p), installed_internal::kHost}));
  const auto plan = Take(Query(provider, triangle, hermitian, a.View(), raw));
  installed_internal::Scratch<T> scratch;
  asc::LapackReport report;
  const auto status = Inverse(provider, triangle, hermitian, a.View(), raw,
                              plan, scratch.workspace, report);
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
  ok = Output(a, before, hermitian, triangle, singular) && ok;
  if (!ok) {
    std::fprintf(
        stderr,
        "installed classic inverse failure N=%zu he=%d tri=%d layout=%d "
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
      for (const int origin : {0, 1, 2}) {
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
        cases += 6;
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
  bool ok = Run<float>(provider, false, cases);
  ok = Run<double>(provider, false, cases) && ok;
  ok = Run<std::complex<float>>(provider, false, cases) && ok;
  ok = Run<std::complex<double>>(provider, false, cases) && ok;
  ok = Run<std::complex<float>>(provider, true, cases) && ok;
  ok = Run<std::complex<double>>(provider, true, cases) && ok;
  std::printf("installed classic inverse cases=%d passed=%d\n", cases,
              static_cast<int>(ok));
  return ok ? 0 : 1;
}
