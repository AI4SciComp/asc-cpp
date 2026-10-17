#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>

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
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Conjugate;
using installed_internal::kColumn;
using installed_internal::kHost;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Scratch;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Wide;
using installed_internal::Widen;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kFamily = asc::LapackFactorFamily::kRook;

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

template <typename T, std::size_t N>
T Original(std::size_t i, std::size_t j, bool hermitian) {
  if constexpr (N == 2) {
    // Exactly one nonsingular 2x2 D block: no multipliers or permutation
    // ambiguity. A is not positive definite, and complex symmetry is not HE.
    if (i == j) {
      return T{};
    }
  } else if (i == j) {
    return Value<T>(i == 1 ? -2 : 4, hermitian ? 0 : 0.125);
  }
  T value = Value<T>(0.25, 0.125);
  if constexpr (N == 2) {
    value = Value<T>(2, 0.5);
  }
  return hermitian && i < j ? Conjugate(value) : value;
}

template <typename T>
T Solution(std::size_t i, std::size_t j) {
  return Value<T>((static_cast<double>(i) + 1) * 0.25,
                  (static_cast<double>(j) + 1) * 0.125);
}

bool Successful(const asc::Status& status, const asc::LapackReport& report,
                bool active) {
  return status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete &&
         report.called_provider == active &&
         report.native_info.has_value() == active &&
         report.native_info.value_or(0) == 0;
}

template <typename T, std::size_t N>
bool SolveCases(const asc::ReferenceLapackProvider& provider, bool hermitian,
                const asc::ReferenceRookFactorView<T>& factor,
                const Matrix<T, N, N>& a,
                const std::array<asc::index_t, N + 2>& pivots,
                Scratch<T>& scratch, asc::LapackReport& report) {
  const auto saved_factor = a.data;
  const auto saved_pivots = pivots;
  for (const auto rhs_layout : {kColumn, kRow}) {
    Matrix<T, N, 2> b{{}, rhs_layout};
    b.data.fill(Value<T>(-23, 11));
    for (std::size_t i = 0; i < N; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        T value{};
        for (std::size_t k = 0; k < N; ++k) {
          value += Original<T, N>(i, k, hermitian) * Solution<T>(k, j);
        }
        b.At(i, j) = value;
      }
    }
    const auto original_rhs = b.data;
    auto query = [&] {
      if constexpr (asc::DenseBlasComplex<T>) {
        if (hermitian) {
          return asc::QueryHetrsRookWorkspace(provider, factor, b.View());
        }
      }
      return asc::QuerySytrsRookWorkspace(provider, factor, b.View());
    };
    const auto solve_plan = Take(query());
    auto solve = [&] {
      if constexpr (asc::DenseBlasComplex<T>) {
        if (hermitian) {
          return asc::HetrsRook(provider, factor, b.View(), solve_plan,
                                scratch.workspace, report);
        }
      }
      return asc::SytrsRook(provider, factor, b.View(), solve_plan,
                            scratch.workspace, report);
    };
    if (!Successful(solve(), report, N != 0) ||
        !b.PaddingEquals(original_rhs) || a.data != saved_factor ||
        pivots != saved_pivots) {
      return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
      for (std::size_t j = 0; j < 2; ++j) {
        Wide residual{};
        for (std::size_t k = 0; k < N; ++k) {
          residual +=
              Widen(Original<T, N>(i, k, hermitian)) * Widen(b.At(k, j));
        }
        if (!Near<T>(Widen(b.At(i, j)), Widen(Solution<T>(i, j)), 4) ||
            !Near<T>(residual, Widen(original_rhs[b.Offset(i, j)]), 16)) {
          return false;
        }
      }
    }
  }
  return true;
}

template <typename T, std::size_t N>
bool Case(const asc::ReferenceLapackProvider& provider, bool hermitian,
          bool blocked, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout layout) {
  Matrix<T, N, N> a{{}, layout};
  a.data.fill(Value<T>(-19, 7));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if ((triangle == kUpper && i <= j) || (triangle == kLower && i >= j)) {
        a.At(i, j) = Original<T, N>(i, j, hermitian);
        if constexpr (asc::DenseBlasComplex<T>) {
          if (hermitian && i == j) {
            a.At(i, j).imag(77);
          }
        }
      }
    }
  }
  const auto before = a.data;
  std::array<asc::index_t, N + 2> pivots{};
  pivots.fill(-31);
  const auto pivot_view = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto plan = Take(QueryFactor(provider, hermitian, blocked, triangle,
                                     a.View(), pivot_view));
  Scratch<T> scratch;
  asc::LapackReport report;
  if (!Successful(Factor(provider, hermitian, blocked, triangle, a.View(),
                         pivot_view, plan, scratch.workspace, report),
                  report, N != 0) ||
      report.factor_family != kFamily || !a.PaddingEquals(before) ||
      pivots.front() != -31 || pivots.back() != -31) {
    return false;
  }
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if ((triangle == kUpper && i > j) || (triangle == kLower && i < j)) {
        if (a.At(i, j) != before[a.Offset(i, j)]) {
          return false;
        }
      }
    }
  }
  if constexpr (N == 2) {
    const asc::index_t encoded = -1;
    if (pivots[1] != encoded || pivots[2] != -2 || a.At(0, 0) != T{} ||
        a.At(1, 1) != T{}) {
      return false;
    }
    const std::size_t i = triangle == kUpper ? 0 : 1;
    const std::size_t j = 1 - i;
    // U/L is identity here, so the selected D block reconstructs A exactly.
    if (a.At(i, j) != Original<T, N>(i, j, hermitian)) {
      return false;
    }
  }
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, N, kFamily, {pivots.data(), sizeof(pivots), kHost}));
  const auto factor = Take(asc::ReferenceRookFactorView<T>::Create(
      provider, a.ConstView(), triangle,
      hermitian ? asc::LapackBunchKaufmanSymmetry::kHermitian
                : asc::LapackBunchKaufmanSymmetry::kSymmetric,
      raw, report));
  return SolveCases(provider, hermitian, factor, a, pivots, scratch, report);
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool hermitian) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const bool blocked : {false, true}) {
        if (!Case<T, 0>(provider, hermitian, blocked, triangle, layout) ||
            !Case<T, 1>(provider, hermitian, blocked, triangle, layout) ||
            !Case<T, 2>(provider, hermitian, blocked, triangle, layout) ||
            !Case<T, 3>(provider, hermitian, blocked, triangle, layout)) {
          return false;
        }
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (!Run<float>(provider, false) || !Run<double>(provider, false) ||
      !Run<std::complex<float>>(provider, false) ||
      !Run<std::complex<double>>(provider, false) ||
      !Run<std::complex<float>>(provider, true) ||
      !Run<std::complex<double>>(provider, true)) {
    std::fputs("Installed classic indefinite consumer failed\n", stderr);
    return 1;
  }
}
