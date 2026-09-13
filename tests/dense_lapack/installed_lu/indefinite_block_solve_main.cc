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
#include "asc/dense/providers/lapack_indefinite.h"
#include "asc/dense/providers/lapack_indefinite_block_solve.h"
#include "asc/dense/providers/lapack_indefinite_driver.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;

template <typename T>
struct Scratch {
  std::array<T, 5000> scalar{};
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

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> factors,
           asc::RawLapackPivotView pivots, asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetrs2Workspace(provider, triangle, factors, pivots,
                                       rhs);
    }
  }
  return asc::QuerySytrs2Workspace(provider, triangle, factors, pivots, rhs);
}
template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, bool hermitian,
                  asc::DenseBlasMatrixView<const T> factors,
                  asc::RawLapackPivotView pivots,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hetrs2(provider, triangle, factors, pivots, rhs, plan,
                         workspace, report);
    }
  }
  return asc::Sytrs2(provider, triangle, factors, pivots, rhs, plan, workspace,
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
  Scratch<T> scratch;
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

bool EqualBytes(const void* first, const void* second, std::size_t size) {
  return std::memcmp(first, second, size) == 0;
}
template <typename T>
T Expected(std::size_t i, std::size_t j) {
  return installed_internal::Value<T>(1 + static_cast<double>((i + j) % 3) / 8,
                                      static_cast<double>((2 * i + j) % 3) / 8);
}
template <typename T, std::size_t N, std::size_t R>
void FillRhs(installed_internal::Matrix<T, N, R>& rhs, bool hermitian,
             bool singular) {
  rhs.data.fill(T{-43});
  for (std::size_t j = 0; j < R; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      installed_internal::Wide sum{};
      for (std::size_t k = 0; k < N; ++k) {
        sum += installed_internal::Widen(
                   Coefficient<T, N>(i, k, hermitian, singular)) *
               installed_internal::Widen(Expected<T>(k, j));
      }
      rhs.At(i, j) = installed_internal::Value<T>(
          static_cast<double>(sum.real()), static_cast<double>(sum.imag()));
    }
  }
}
template <typename T, std::size_t N, std::size_t R>
bool Output(const installed_internal::Matrix<T, N, R>& rhs,
            const installed_internal::Matrix<T, N, R>& before, bool hermitian,
            bool singular) {
  using Real = asc::DenseBlasRealType<T>;
  bool ok = true;
  for (std::size_t k = 0; k < rhs.data.size(); ++k) {
    bool selected = false;
    for (std::size_t j = 0; j < R; ++j) {
      for (std::size_t i = 0; i < N; ++i) {
        selected = selected || k == rhs.Offset(i, j);
      }
    }
    if (!selected) {
      ok = EqualBytes(&rhs.data[k], &before.data[k], sizeof(T)) && ok;
    }
  }
  if (singular) {
    return ok;
  }
  for (std::size_t j = 0; j < R; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      const auto value = installed_internal::Widen(rhs.At(i, j));
      const auto expected = installed_internal::Widen(Expected<T>(i, j));
      ok =
          std::isfinite(value.real()) && std::isfinite(value.imag()) &&
          std::abs(value - expected) <=
              128 * std::numeric_limits<Real>::epsilon() * std::abs(expected) &&
          ok;
      installed_internal::Wide sum{};
      long double magnitude =
          std::abs(installed_internal::Widen(before.At(i, j)));
      for (std::size_t k = 0; k < N; ++k) {
        const auto term = installed_internal::Widen(
                              Coefficient<T, N>(i, k, hermitian, false)) *
                          installed_internal::Widen(rhs.At(k, j));
        sum += term;
        magnitude += std::abs(term);
      }
      ok = std::abs(sum - installed_internal::Widen(before.At(i, j))) <=
               128 * std::numeric_limits<Real>::epsilon() * magnitude &&
           ok;
    }
  }
  return ok;
}
template <typename T, std::size_t N, std::size_t R>
bool Check(const asc::ReferenceLapackProvider& provider, bool hermitian,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           asc::DenseBlasLayout rhs_layout, int origin, bool singular) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  a.data.fill(T{-37});
  for (std::size_t j = 0; j < N; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      if (triangle == asc::DenseBlasTriangle::kUpper ? i <= j : i >= j) {
        a.At(i, j) = Coefficient<T, N>(i, j, hermitian, singular);
      }
    }
  }
  installed_internal::Matrix<T, N, R> rhs{{}, rhs_layout};
  FillRhs(rhs, hermitian, singular);
  const auto before_rhs = rhs;
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
  const auto plan = Take(
      Query(provider, triangle, hermitian, a.ConstView(), raw, rhs.View()));
  Scratch<T> scratch;
  asc::LapackReport report;
  const auto status = Solve(provider, triangle, hermitian, a.ConstView(), raw,
                            rhs.View(), plan, scratch.workspace, report);
  constexpr bool kActive = N != 0 && R != 0;
  ok = status.ok() && report.called_provider == kActive &&
       report.native_info.has_value() == kActive && p == old_p &&
       EqualBytes(a.data.data(), before.data.data(), sizeof(a.data)) && ok;
  if constexpr (kActive) {
    ok = report.native_info == 0 && ok;
  }
  ok = report.outcome == asc::LapackOutcome::kSuccess &&
       report.output_validity == asc::LapackOutputValidity::kComplete &&
       !report.diagnostic_index.has_value() &&
       !report.native_argument.has_value() && ok;
  ok = Output(rhs, before_rhs, hermitian, singular) && ok;
  if (!ok) {
    std::fprintf(stderr,
                 "installed block solve failure N=%zu NRHS=%zu he=%d tri=%d "
                 "A_layout=%d B_layout=%d origin=%d singular=%d\n",
                 N, R, static_cast<int>(hermitian), static_cast<int>(triangle),
                 static_cast<int>(layout), static_cast<int>(rhs_layout), origin,
                 static_cast<int>(singular));
  }
  return ok;
}
template <typename T, std::size_t R>
bool RunRhs(const asc::ReferenceLapackProvider& provider, bool hermitian,
            int& cases) {
  bool ok = true;
  for (const auto triangle :
       {asc::DenseBlasTriangle::kUpper, asc::DenseBlasTriangle::kLower}) {
    for (const auto al :
         {installed_internal::kColumn, installed_internal::kRow}) {
      for (const auto bl :
           {installed_internal::kColumn, installed_internal::kRow}) {
        for (const int origin : {0, 1, 2}) {
          ok = Check<T, 0, R>(provider, hermitian, triangle, al, bl, origin,
                              false) &&
               ok;
          ok = Check<T, 1, R>(provider, hermitian, triangle, al, bl, origin,
                              false) &&
               ok;
          ok = Check<T, 1, R>(provider, hermitian, triangle, al, bl, origin,
                              true) &&
               ok;
          ok = Check<T, 2, R>(provider, hermitian, triangle, al, bl, origin,
                              false) &&
               ok;
          ok = Check<T, 7, R>(provider, hermitian, triangle, al, bl, origin,
                              false) &&
               ok;
          ok = Check<T, 7, R>(provider, hermitian, triangle, al, bl, origin,
                              true) &&
               ok;
          ok = Check<T, 67, R>(provider, hermitian, triangle, al, bl, origin,
                               false) &&
               ok;
          ok = Check<T, 67, R>(provider, hermitian, triangle, al, bl, origin,
                               true) &&
               ok;
          cases += 8;
        }
      }
    }
  }
  return ok;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool hermitian,
         int& cases) {
  bool ok = RunRhs<T, 0>(provider, hermitian, cases);
  ok = RunRhs<T, 1>(provider, hermitian, cases) && ok;
  ok = RunRhs<T, 3>(provider, hermitian, cases) && ok;
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
  std::printf("installed block solve cases=%d passed=%d\n", cases,
              static_cast<int>(ok));
  return ok ? 0 : 1;
}
