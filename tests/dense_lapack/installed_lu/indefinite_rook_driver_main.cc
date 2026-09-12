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
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "asc/dense/providers/lapack_indefinite_rook_driver.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasVectorView<asc::index_t> pivots,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHesvRookWorkspace(provider, triangle, a, pivots, b);
    }
  }
  return asc::QuerySysvRookWorkspace(provider, triangle, a, pivots, b);
}

template <typename T>
asc::Status Driver(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasMatrixView<T> a,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasMatrixView<T> b,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HesvRook(provider, triangle, a, pivots, b, plan, workspace,
                           report);
    }
  }
  return asc::SysvRook(provider, triangle, a, pivots, b, plan, workspace,
                       report);
}

template <typename T>
auto QuerySolve(const asc::ReferenceLapackProvider& provider, bool hermitian,
                const asc::ReferenceRookFactorView<T>& factor,
                asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetrsRookWorkspace(provider, factor, rhs);
    }
  }
  return asc::QuerySytrsRookWorkspace(provider, factor, rhs);
}

template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider, bool hermitian,
                  const asc::ReferenceRookFactorView<T>& factor,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HetrsRook(provider, factor, rhs, plan, workspace, report);
    }
  }
  return asc::SytrsRook(provider, factor, rhs, plan, workspace, report);
}

template <typename T, std::size_t N, std::size_t Rhs>
bool Reuse(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, bool singular,
           installed_internal::Matrix<T, N, N>& a,
           installed_internal::Matrix<T, N, Rhs>& b,
           const std::array<T, (N + 1) * (Rhs + 1)>& old_b,
           const std::array<asc::index_t, N + 2>& p,
           const asc::LapackReport& report) {
  using Real = asc::DenseBlasRealType<T>;
  bool ok = true;
  const auto raw = Take(asc::RawLapackPivotView::Create(
      p.data() + 1, N, asc::LapackFactorFamily::kRook,
      {p.data(), sizeof(p), installed_internal::kHost}));
  const auto factor = asc::ReferenceRookFactorView<T>::Create(
      provider, a.ConstView(), triangle,
      hermitian ? asc::LapackBunchKaufmanSymmetry::kHermitian
                : asc::LapackBunchKaufmanSymmetry::kSymmetric,
      raw, report);
  ok = ok && factor.ok() != singular;
  if (singular) {
    ok = ok && report.outcome == asc::LapackOutcome::kSingular &&
         report.output_validity ==
             asc::LapackOutputValidity::kDocumentedPartial &&
         b.data == old_b;
  } else {
    ok = ok && report.output_validity == asc::LapackOutputValidity::kComplete;
    for (std::size_t j = 0; j < Rhs; ++j) {
      for (std::size_t i = 0; i < N; ++i) {
        ok = ok && std::abs(b.At(i, j) - T{1}) <=
                       32 * std::numeric_limits<Real>::epsilon();
      }
    }
    if (factor.ok()) {
      ok = ok && factor->originating_routine().ends_with("sv_rook");
      const auto solution = b.data;
      const auto factors = a.data;
      const auto saved_p = p;
      b.data = old_b;
      const auto reuse_plan =
          Take(QuerySolve(provider, hermitian, *factor, b.View()));
      installed_internal::Scratch<T> reuse_scratch;
      asc::LapackReport reuse_report;
      const auto reuse =
          Solve(provider, hermitian, *factor, b.View(), reuse_plan,
                reuse_scratch.workspace, reuse_report);
      ok = ok && reuse.ok() &&
           reuse_report.called_provider == (N != 0 && Rhs != 0) &&
           b.data == solution && a.data == factors && p == saved_p;
    }
  }
  return ok;
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
  std::array<asc::index_t, N + 2> p{};
  p.fill(-47);
  const auto pivots = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      p.data() + 1, N, 1, {p.data(), sizeof(p), installed_internal::kHost}));
  const auto plan =
      Take(Query(provider, triangle, hermitian, a.View(), pivots, b.View()));
  installed_internal::Scratch<T> scratch;
  if (!preferred) {
    scratch.workspace.regions[static_cast<std::size_t>(
        asc::LapackWorkspaceKind::kScalar)] = {scratch.scalar.data(), sizeof(T),
                                               installed_internal::kHost};
  }
  asc::LapackReport report;
  const auto status = Driver(provider, triangle, hermitian, a.View(), pivots,
                             b.View(), plan, scratch.workspace, report);
  bool ok = status.ok() != singular && report.called_provider == (N != 0) &&
            report.factor_family == asc::LapackFactorFamily::kRook &&
            p.front() == -47 && p.back() == -47 && a.PaddingEquals(old_a) &&
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
  ok = Reuse(provider, hermitian, triangle, singular, a, b, old_b, p, report) &&
       ok;
  if (!ok) {
    std::fprintf(stderr,
                 "installed rook driver failure N=%zu NRHS=%zu he=%d uplo=%d "
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
  std::printf("installed rook driver cases=768 failures=%d\n", failed);
  return failed == 0 ? 0 : 1;
}
