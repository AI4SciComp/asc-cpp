#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
namespace support = installed_internal;

template <typename T>
bool SolveCases(const asc::ReferenceLapackProvider& provider,
                asc::ReferenceLuBandFactorView<T> factor,
                const std::array<T, 26>& band,
                const std::array<asc::index_t, 3>& pivots,
                const std::array<T, 9>& a) {
  const auto factored = band;
  const auto pivot_before = pivots;
  const T guard = support::Value<T>(-163, 53);
  support::Scratch<T> scratch;
  for (const auto layout : {support::kColumn, support::kRow}) {
    for (const auto operation :
         {asc::DenseBlasTranspose::kNone, asc::DenseBlasTranspose::kTranspose,
          asc::DenseBlasTranspose::kConjugateTranspose}) {
      support::Matrix<T, 3, 2> rhs{{}, layout};
      rhs.data.fill(guard);
      const std::array<T, 6> expected{support::Value<T>(1, 1),  T{-1}, T{2},
                                      support::Value<T>(0, -1), T{1},  T{3}};
      std::array<support::Wide, 6> b{};
      const auto coefficient = [&](std::size_t i, std::size_t j) {
        const auto entry =
            a[operation == asc::DenseBlasTranspose::kNone ? 3 * i + j
                                                          : 3 * j + i];
        return support::Widen(
            operation == asc::DenseBlasTranspose::kConjugateTranspose
                ? support::Conjugate(entry)
                : entry);
      };
      for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 2; ++j) {
          for (std::size_t k = 0; k < 3; ++k) {
            b[2 * i + j] +=
                coefficient(i, k) * support::Widen(expected[2 * k + j]);
          }
          rhs.At(i, j) =
              support::Value<T>(static_cast<double>(b[2 * i + j].real()),
                                static_cast<double>(b[2 * i + j].imag()));
        }
      }
      const auto before = rhs.data;
      const auto solve_plan = support::Take(
          asc::QueryGbtrsWorkspace(provider, operation, factor, rhs.View()));
      asc::LapackReport solve_report;
      if (!support::Succeeded(
              asc::Gbtrs(provider, operation, factor, rhs.View(), solve_plan,
                         scratch.workspace, solve_report),
              solve_report) ||
          band != factored || pivots != pivot_before ||
          !rhs.PaddingEquals(before)) {
        return false;
      }
      for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 2; ++j) {
          support::Wide residual = -b[2 * i + j];
          long double scale = std::abs(b[2 * i + j]);
          for (std::size_t k = 0; k < 3; ++k) {
            const auto product =
                coefficient(i, k) * support::Widen(rhs.At(k, j));
            residual += product;
            scale += std::abs(product);
          }
          if (!support::Near<T>(support::Widen(rhs.At(i, j)),
                                support::Widen(expected[2 * i + j]), 4) ||
              !support::Near<T>(residual, {}, scale)) {
            return false;
          }
        }
      }
      const auto solved = rhs.data;
      auto stale = solve_plan;
      stale
          .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)]
          .minimum_entries = 0;
      const auto rejected = asc::Gbtrs(provider, operation, factor, rhs.View(),
                                       stale, scratch.workspace, solve_report);
      if (rejected.code() != asc::ErrorCode::kInvalidState ||
          solve_report.called_provider ||
          solve_report.native_info.has_value() || rhs.data != solved) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  // Column-major band with KL=1, KU=2, three fill/padding guard rows. The
  // 2x2 leading pivot block is nonsingular and forces an actual row swap.
  constexpr asc::extent_t kN = 3;
  constexpr asc::extent_t kLd = 8;
  const T guard = support::Value<T>(-163, 53);
  std::array<T, kN * kLd + 2> band{};
  band.fill(guard);
  const std::array<T, 9> a{
      T{}, support::Value<T>(2, 1), T{1}, support::Value<T>(3, -1), T{1}, T{-1},
      T{}, support::Value<T>(1, 1), T{4}};
  for (asc::extent_t j = 0; j < kN; ++j) {
    for (asc::extent_t i = std::max<asc::extent_t>(0, j - 2);
         i < std::min(kN, j + 2); ++i) {
      band[static_cast<std::size_t>(1 + j * kLd + 3 + i - j)] =
          a[static_cast<std::size_t>(3 * i + j)];
    }
  }
  const auto matrix = support::Take(asc::LapackLuBandView<T>::Create(
      band.data() + 1, kN, kN, 1, 2, kLd,
      {band.data(), sizeof(band), support::kHost}));
  std::array<asc::index_t, 3> pivots{};
  const auto swaps = support::Vector(pivots);
  const auto plan =
      support::Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
  support::Scratch<T> scratch;
  asc::LapackReport report;
  const auto status =
      asc::Gbtrf(provider, matrix, swaps, plan, scratch.workspace, report);
  if (!support::Succeeded(status, report) || report.factor_family.has_value() ||
      pivots[0] != 2) {
    return false;
  }
  const auto const_band = support::Take(asc::LapackLuBandView<const T>::Create(
      band.data() + 1, kN, kN, 1, 2, kLd,
      {band.data(), sizeof(band), support::kHost}));
  const auto factor = support::Take(asc::ReferenceLuBandFactorView<T>::Create(
      provider, const_band, support::ConstVector(pivots), report));
  if (band.front() != guard || band.back() != guard) {
    return false;
  }
  for (asc::extent_t j = 0; j < kN; ++j) {
    for (asc::extent_t i = 5; i < kLd; ++i) {
      if (band[static_cast<std::size_t>(1 + j * kLd + i)] != guard) {
        return false;
      }
    }
  }
  return SolveCases(provider, factor, band, pivots, a);
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = support::Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (!Run<float>(provider) || !Run<double>(provider) ||
      !Run<std::complex<float>>(provider) ||
      !Run<std::complex<double>>(provider)) {
    return 1;
  }
  std::puts(
      "Public GBTRF/GBTRS consumer: all eight routes, N/T/C, independent RHS "
      "layouts, reuse, guards and rejections passed.");
  return 0;
}
