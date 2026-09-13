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
#include "asc/dense/providers/lapack_indefinite_rk_condition.h"
#include "asc/dense/providers/lapack_indefinite_rk_driver.h"
#include "asc/dense/providers/lapack_indefinite_rk_inverse.h"
#include "asc/dense/providers/lapack_indefinite_rk_solve.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> a, asc::DenseBlasVectorView<T> extra,
           asc::DenseBlasVectorView<asc::index_t> pivots,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHesvRkWorkspace(provider, triangle, a, extra, pivots, b);
    }
  }
  return asc::QuerySysvRkWorkspace(provider, triangle, a, extra, pivots, b);
}

template <typename T>
asc::Status Driver(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasMatrixView<T> a,
                   asc::DenseBlasVectorView<T> extra,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasMatrixView<T> b,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HesvRk(provider, triangle, a, extra, pivots, b, plan,
                         workspace, report);
    }
  }
  return asc::SysvRk(provider, triangle, a, extra, pivots, b, plan, workspace,
                     report);
}

template <typename T>
auto QuerySolve(const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasTriangle triangle, bool hermitian,
                asc::DenseBlasMatrixView<const T> factors,
                asc::DenseBlasVectorView<const T> off_diagonal,
                asc::RawLapackPivotView pivots,
                asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetrs3Workspace(provider, triangle, factors,
                                       off_diagonal, pivots, rhs);
    }
  }
  return asc::QuerySytrs3Workspace(provider, triangle, factors, off_diagonal,
                                   pivots, rhs);
}
template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, bool hermitian,
                  asc::DenseBlasMatrixView<const T> factors,
                  asc::DenseBlasVectorView<const T> off_diagonal,
                  asc::RawLapackPivotView pivots,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hetrs3(provider, triangle, factors, off_diagonal, pivots, rhs,
                         plan, workspace, report);
    }
  }
  return asc::Sytrs3(provider, triangle, factors, off_diagonal, pivots, rhs,
                     plan, workspace, report);
}
template <typename T>
auto QueryCondition(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    asc::DenseBlasMatrixView<const T> a,
                    asc::DenseBlasVectorView<const T> extra,
                    asc::RawLapackPivotView pivots,
                    asc::DenseBlasRealType<T> norm,
                    const asc::DenseBlasRealType<T>& condition) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHecon3Workspace(provider, triangle, a, extra, pivots,
                                       norm, condition);
    }
  }
  return asc::QuerySycon3Workspace(provider, triangle, a, extra, pivots, norm,
                                   condition);
}

template <typename T>
asc::Status Condition(const asc::ReferenceLapackProvider& provider,
                      asc::DenseBlasTriangle triangle, bool hermitian,
                      asc::DenseBlasMatrixView<const T> a,
                      asc::DenseBlasVectorView<const T> extra,
                      asc::RawLapackPivotView pivots,
                      asc::DenseBlasRealType<T> norm,
                      asc::DenseBlasRealType<T>& condition,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hecon3(provider, triangle, a, extra, pivots, norm, condition,
                         plan, workspace, report);
    }
  }
  return asc::Sycon3(provider, triangle, a, extra, pivots, norm, condition,
                     plan, workspace, report);
}

template <typename T>
bool Invert(const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasTriangle triangle, bool hermitian,
            asc::DenseBlasMatrixView<T> matrix,
            asc::DenseBlasVectorView<const T> extra,
            asc::RawLapackPivotView pivots) {
  installed_internal::Scratch<T> scratch;
  asc::LapackReport report;
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      const auto plan = Take(
          asc::QueryHetri3Workspace(provider, triangle, matrix, extra, pivots));
      return asc::Hetri3(provider, triangle, matrix, extra, pivots, plan,
                         scratch.workspace, report)
          .ok();
    }
  }
  const auto plan = Take(
      asc::QuerySytri3Workspace(provider, triangle, matrix, extra, pivots));
  return asc::Sytri3(provider, triangle, matrix, extra, pivots, plan,
                     scratch.workspace, report)
      .ok();
}

template <typename T, std::size_t N>
bool Diagnostics(const asc::ReferenceLapackProvider& provider, bool hermitian,
                 asc::DenseBlasTriangle triangle,
                 installed_internal::Matrix<T, N, N>& a,
                 asc::DenseBlasVectorView<const T> extra,
                 asc::RawLapackPivotView pivots) {
  using Real = asc::DenseBlasRealType<T>;
  Real norm = 0;
  if constexpr (N == 1) {
    norm = 4;
  } else if constexpr (N == 2) {
    norm = std::abs(installed_internal::Value<T>(3, 4));
  }
  Real condition = -1;
  const auto plan =
      Take(QueryCondition(provider, triangle, hermitian, a.ConstView(), extra,
                          pivots, norm, condition));
  installed_internal::Scratch<T> scratch;
  asc::LapackReport report;
  const auto saved_a = a.data;
  const auto status =
      Condition(provider, triangle, hermitian, a.ConstView(), extra, pivots,
                norm, condition, plan, scratch.workspace, report);
  bool ok =
      status.ok() && a.data == saved_a && std::isfinite(condition) &&
      std::abs(condition - 1) <= 32 * std::numeric_limits<Real>::epsilon();
  const bool inverted =
      Invert(provider, triangle, hermitian, a.View(), extra, pivots);
  ok = inverted && ok;
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      if (triangle == asc::DenseBlasTriangle::kUpper ? i > j : i < j) {
        ok = (a.At(i, j) == saved_a[a.Offset(i, j)]) && ok;
        continue;
      }
      T expected{};
      if constexpr (N == 1) {
        expected = T{0.25};
      }
      if constexpr (N == 2) {
        if (i != j) {
          const auto coefficient =
              installed_internal::Value<T>(3, hermitian && i == 1 ? -4 : 4);
          expected = T{1} / coefficient;
        }
      }
      ok =
          installed_internal::Near<T>(installed_internal::Widen(a.At(i, j)),
                                      installed_internal::Widen(expected), 1) &&
          ok;
      if constexpr (asc::DenseBlasComplex<T>) {
        if (hermitian && i == j) {
          ok = (a.At(i, j).imag() == Real{}) && ok;
        }
      }
    }
  }
  return a.PaddingEquals(saved_a) && ok;
}

template <typename T, std::size_t N, std::size_t Rhs>
bool Reuse(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, bool singular,
           installed_internal::Matrix<T, N, N>& a,
           installed_internal::Matrix<T, N, Rhs>& b,
           const std::array<T, (N + 1) * (Rhs + 1)>& old_b,
           const std::array<T, N + 2>& e,
           const std::array<asc::index_t, N + 2>& p,
           const asc::LapackReport& report) {
  using Real = asc::DenseBlasRealType<T>;
  if (singular) {
    return report.outcome == asc::LapackOutcome::kSingular &&
           report.output_validity ==
               asc::LapackOutputValidity::kDocumentedPartial &&
           b.data == old_b;
  }
  bool ok = report.output_validity == asc::LapackOutputValidity::kComplete;
  for (std::size_t j = 0; j < Rhs; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      ok = (std::abs(b.At(i, j) - T{1}) <=
            32 * std::numeric_limits<Real>::epsilon()) &&
           ok;
    }
  }
  const auto raw = Take(asc::RawLapackPivotView::Create(
      p.data() + 1, N, asc::LapackFactorFamily::kRook,
      {p.data(), sizeof(p), installed_internal::kHost}));
  const auto extra = Take(asc::DenseBlasVectorView<const T>::Create(
      e.data() + 1, N, 1, {e.data(), sizeof(e), installed_internal::kHost}));
  const auto solution = b.data;
  const auto factors = a.data;
  const auto saved_p = p;
  const auto saved_e = e;
  b.data = old_b;
  const auto plan = Take(QuerySolve(provider, triangle, hermitian,
                                    a.ConstView(), extra, raw, b.View()));
  installed_internal::Scratch<T> scratch;
  asc::LapackReport solve_report;
  const auto status =
      Solve(provider, triangle, hermitian, a.ConstView(), extra, raw, b.View(),
            plan, scratch.workspace, solve_report);
  ok = status.ok() && solve_report.called_provider == (N != 0 && Rhs != 0) &&
       b.data == solution && a.data == factors && p == saved_p &&
       e == saved_e && ok;
  const bool diagnostics =
      Diagnostics(provider, hermitian, triangle, a, extra, raw);
  return diagnostics && e == saved_e && p == saved_p && ok;
}

template <typename T, std::size_t N, std::size_t Rhs>
bool Check(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           asc::DenseBlasLayout rhs_layout, bool preferred, bool singular) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  installed_internal::Matrix<T, N, Rhs> b{{}, rhs_layout};
  a.data.fill(T{-37});
  b.data.fill(T{-41});
  if constexpr (N == 1) {
    a.At(0, 0) = singular ? T{} : T{4};
  }
  if constexpr (N == 2) {
    a.At(0, 0) = T{};
    a.At(1, 1) = T{};
    a.At(1, 0) = installed_internal::Value<T>(3, 4);
    a.At(0, 1) = installed_internal::Value<T>(3, hermitian ? -4 : 4);
  }
  for (std::size_t j = 0; j < Rhs; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      b.At(i, j) = T{};
      for (std::size_t k = 0; k < N; ++k) {
        b.At(i, j) += a.At(i, k);
      }
    }
  }
  const auto old_a = a.data;
  const auto old_b = b.data;
  std::array<T, N + 2> e{};
  e.fill(T{-53});
  const auto extra = Take(asc::DenseBlasVectorView<T>::Create(
      e.data() + 1, N, 1, {e.data(), sizeof(e), installed_internal::kHost}));
  std::array<asc::index_t, N + 2> p{};
  p.fill(-47);
  const auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      p.data() + 1, N, 1, {p.data(), sizeof(p), installed_internal::kHost}));
  const auto plan = Take(
      Query(provider, triangle, hermitian, a.View(), extra, pivots, b.View()));
  installed_internal::Scratch<T> scratch;
  if (!preferred) {
    scratch.workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kScalar)] = {scratch.scalar.data(), sizeof(T),
                                               installed_internal::kHost};
  }
  asc::LapackReport report;
  const auto status = Driver(provider, triangle, hermitian, a.View(), extra,
                             pivots, b.View(), plan, scratch.workspace, report);
  bool ok = status.ok() != singular && report.called_provider == (N != 0) &&
            report.factor_family == asc::LapackFactorFamily::kRook &&
            p.front() == -47 && p.back() == -47 && e.front() == T{-53} &&
            e.back() == T{-53} && a.PaddingEquals(old_a) &&
            b.PaddingEquals(old_b);
  if constexpr (N == 0) {
    ok = ok && !report.native_info.has_value();
  } else {
    ok = ok && report.native_info == (singular ? 1 : 0);
  }
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      if (triangle == asc::DenseBlasTriangle::kUpper ? i > j : i < j) {
        ok = ok && a.At(i, j) == old_a[a.Offset(i, j)];
      }
    }
  }
  ok = Reuse(provider, hermitian, triangle, singular, a, b, old_b, e, p,
             report) &&
       ok;
  if (!ok) {
    std::fprintf(stderr,
                 "installed RK driver failure N=%zu NRHS=%zu he=%d uplo=%d "
                 "A=%d B=%d preferred=%d singular=%d\n",
                 N, Rhs, hermitian, static_cast<int>(triangle),
                 static_cast<int>(layout), static_cast<int>(rhs_layout),
                 preferred, singular);
  }
  return ok;
}
template <typename T>
int Run(const asc::ReferenceLapackProvider& provider, bool hermitian) {
  int failed = 0;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto layout :
         {installed_internal::kColumn, installed_internal::kRow}) {
      for (const auto rhs_layout :
           {installed_internal::kColumn, installed_internal::kRow}) {
        for (const bool preferred : {false, true}) {
          failed += !Check<T, 0, 0>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, false);
          failed += !Check<T, 0, 2>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, false);
          failed += !Check<T, 1, 0>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, false);
          failed += !Check<T, 1, 2>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, false);
          failed += !Check<T, 1, 0>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, true);
          failed += !Check<T, 1, 2>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, true);
          failed += !Check<T, 2, 0>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, false);
          failed += !Check<T, 2, 2>(provider, hermitian, triangle, layout,
                                    rhs_layout, preferred, false);
        }
      }
    }
  }
  return failed;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const int failed = Run<float>(provider, false) +
                     Run<double>(provider, false) +
                     Run<std::complex<float>>(provider, false) +
                     Run<std::complex<double>>(provider, false) +
                     Run<std::complex<float>>(provider, true) +
                     Run<std::complex<double>>(provider, true);
  std::printf("installed RK driver cases=768 failures=%d\n", failed);
  return failed == 0 ? 0 : 1;
}
