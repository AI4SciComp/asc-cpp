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
#include "asc/dense/providers/lapack_indefinite_rook.h"
#include "asc/dense/providers/lapack_indefinite_rook_condition.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider, bool hermitian,
                 bool blocked, asc::DenseBlasTriangle triangle,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::QueryHetrfRookWorkspace(provider, triangle, matrix,
                                                    pivots)
                     : asc::QueryHetf2RookWorkspace(provider, triangle, matrix,
                                                    pivots);
    }
  }
  return blocked
             ? asc::QuerySytrfRookWorkspace(provider, triangle, matrix, pivots)
             : asc::QuerySytf2RookWorkspace(provider, triangle, matrix, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider, bool hermitian,
                   bool blocked, asc::DenseBlasTriangle triangle,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::HetrfRook(provider, triangle, matrix, pivots, plan,
                                      workspace, report)
                     : asc::Hetf2Rook(provider, triangle, matrix, pivots, plan,
                                      workspace, report);
    }
  }
  return blocked ? asc::SytrfRook(provider, triangle, matrix, pivots, plan,
                                  workspace, report)
                 : asc::Sytf2Rook(provider, triangle, matrix, pivots, plan,
                                  workspace, report);
}

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> a, asc::RawLapackPivotView pivots,
           asc::DenseBlasRealType<T> norm,
           const asc::DenseBlasRealType<T>& condition) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHeconRookWorkspace(provider, triangle, a, pivots, norm,
                                          condition);
    }
  }
  return asc::QuerySyconRookWorkspace(provider, triangle, a, pivots, norm,
                                      condition);
}

template <typename T>
asc::Status Condition(const asc::ReferenceLapackProvider& provider,
                      asc::DenseBlasTriangle triangle, bool hermitian,
                      asc::DenseBlasMatrixView<const T> a,
                      asc::RawLapackPivotView pivots,
                      asc::DenseBlasRealType<T> norm,
                      asc::DenseBlasRealType<T>& condition,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HeconRook(provider, triangle, a, pivots, norm, condition,
                            plan, workspace, report);
    }
  }
  return asc::SyconRook(provider, triangle, a, pivots, norm, condition, plan,
                        workspace, report);
}

template <typename T, std::size_t N>
bool Check(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           bool singular) {
  using Real = asc::DenseBlasRealType<T>;
  installed_internal::Matrix<T, N, N> a{{}, layout};
  if constexpr (N == 1) {
    a.At(0, 0) = singular ? T{} : T{4};
  }
  if constexpr (N == 2) {
    a.At(1, 0) = installed_internal::Value<T>(3, 4);
    a.At(0, 1) = installed_internal::Value<T>(3, hermitian ? -4 : 4);
  }
  std::array<asc::index_t, N + 2> pivots{};
  const auto pivot = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1,
      {pivots.data(), sizeof(pivots), installed_internal::kHost}));
  const auto fp =
      Take(QueryFactor(provider, hermitian, false, triangle, a.View(), pivot));
  installed_internal::Scratch<T> scratch;
  asc::LapackReport report;
  const auto factored = Factor(provider, hermitian, false, triangle, a.View(),
                               pivot, fp, scratch.workspace, report);
  if (factored.ok() == singular || (singular && report.native_info != 1)) {
    return false;
  }
  const auto saved_a = a.data;
  const auto saved_p = pivots;
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, N, asc::LapackFactorFamily::kRook,
      {pivots.data(), sizeof(pivots), installed_internal::kHost}));
  const Real pair_norm = asc::DenseBlasComplex<T> ? Real{5} : Real{3};
  const Real norm = N == 2 ? pair_norm : Real{4};
  Real condition = -13;
  const auto plan = Take(Query(provider, triangle, hermitian, a.ConstView(),
                               raw, norm, condition));
  const auto status =
      Condition(provider, triangle, hermitian, a.ConstView(), raw, norm,
                condition, plan, scratch.workspace, report);
  const Real expected = singular ? Real{} : Real{1};
  const bool okay =
      status.ok() && report.called_provider == (N != 0) &&
      report.native_info.has_value() == (N != 0) &&
      report.native_info.value_or(0) == 0 &&
      report.output_validity == asc::LapackOutputValidity::kComplete &&
      std::isfinite(condition) &&
      std::abs(condition - expected) <=
          32 * std::numeric_limits<Real>::epsilon() &&
      a.data == saved_a && pivots == saved_p;
  std::printf(
      "installed rook condition n=%zu hermitian=%d singular=%d pass=%d\n", N,
      static_cast<int>(hermitian), static_cast<int>(singular),
      static_cast<int>(okay));
  return okay;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool hermitian) {
  bool passed = true;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto layout :
         {installed_internal::kColumn, installed_internal::kRow}) {
      passed =
          Check<T, 0>(provider, hermitian, triangle, layout, false) && passed;
      passed =
          Check<T, 1>(provider, hermitian, triangle, layout, false) && passed;
      passed =
          Check<T, 1>(provider, hermitian, triangle, layout, true) && passed;
      passed =
          Check<T, 2>(provider, hermitian, triangle, layout, false) && passed;
    }
  }
  return passed;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  bool passed = Run<float>(provider, false);
  passed = Run<double>(provider, false) && passed;
  passed = Run<std::complex<float>>(provider, false) && passed;
  passed = Run<std::complex<double>>(provider, false) && passed;
  passed = Run<std::complex<float>>(provider, true) && passed;
  passed = Run<std::complex<double>>(provider, true) && passed;
  return passed ? 0 : 1;
}
