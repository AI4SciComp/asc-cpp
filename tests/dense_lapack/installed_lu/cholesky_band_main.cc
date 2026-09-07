#include <array>
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
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "factorization_support.h"

namespace {
using namespace installed_internal;  // NOLINT(google-build-using-namespace)
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T, std::size_t N, std::size_t Kd>
struct Band {
  static constexpr std::size_t kLeading = Kd + 3;
  std::array<T, N * kLeading + 2> data{};
  asc::DenseBlasLayout layout;
  asc::DenseBlasTriangle triangle;

  [[nodiscard]] bool Selected(std::size_t i, std::size_t j) const {
    return triangle == kUpper ? i <= j && j - i <= Kd : j <= i && i - j <= Kd;
  }
  [[nodiscard]] std::size_t Offset(std::size_t i, std::size_t j) const {
    if (layout == kColumn) {
      return 1 + j * kLeading + (triangle == kUpper ? Kd - (j - i) : i - j);
    }
    return 1 + i * kLeading + (triangle == kUpper ? j - i : Kd - (i - j));
  }
  T& At(std::size_t i, std::size_t j) { return data[Offset(i, j)]; }
  [[nodiscard]] Wide Factor(std::size_t i, std::size_t j) const {
    return Selected(i, j) ? Widen(data[Offset(i, j)]) : Wide{};
  }
  auto View() {
    return Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        data.data() + 1, N, Kd, triangle, layout, kLeading,
        {data.data(), sizeof(data), kHost}));
  }
  auto ConstView() {
    return Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
        data.data() + 1, N, Kd, triangle, layout, kLeading,
        {data.data(), sizeof(data), kHost}));
  }
  [[nodiscard]] bool PaddingEquals(const decltype(data)& old) const {
    auto expected = old;
    for (std::size_t i = 0; i < N; ++i) {
      for (std::size_t j = 0; j < N; ++j) {
        if (Selected(i, j)) {
          expected[Offset(i, j)] = data[Offset(i, j)];
        }
      }
    }
    return expected == data;
  }
};

template <typename T>
Wide Original(std::size_t i, std::size_t j) {
  // Independently specified tridiagonal A=L*L^H: L has diagonal 2 and
  // subdiagonal phase. The wider band fixture still exercises blocked PBTRF.
  const auto phase = Widen(Value<T>(0.25, 0.125));
  if (i == j) {
    return {4 + (i == 0 ? 0 : std::norm(phase)), 0};
  }
  if (i == j + 1) {
    return 2.0L * phase;
  }
  return j == i + 1 ? 2.0L * std::conj(phase) : Wide{};
}

template <typename T, std::size_t N, std::size_t Kd>
bool Reconstruct(const Band<T, N, Kd>& band) {
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      Wide product{};
      for (std::size_t k = 0; k < N; ++k) {
        product += band.triangle == kLower
                       ? band.Factor(i, k) * std::conj(band.Factor(j, k))
                       : std::conj(band.Factor(k, i)) * band.Factor(k, j);
      }
      if (!Near<T>(product, Original<T>(i, j), 8)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, std::size_t N, std::size_t Kd>
bool Solve(const asc::ReferenceLapackProvider& provider, Band<T, N, Kd>& band,
           asc::DenseBlasLayout rhs_layout, asc::LapackWorkspace& workspace) {
  Matrix<T, N, 2> rhs{{}, rhs_layout};
  rhs.data.fill(Value<T>(-93, 29));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      Wide value{};
      for (std::size_t k = 0; k < N; ++k) {
        value += Original<T>(i, k) * Widen(Value<T>(1 + (k % 3) + j, 0.25));
      }
      rhs.At(i, j) = Value<T>(value.real(), value.imag());
    }
  }
  const auto before = rhs.data;
  const auto factors = band.data;
  const auto plan =
      Take(asc::QueryPbtrsWorkspace(provider, band.ConstView(), rhs.View()));
  asc::LapackReport report;
  if (rhs.data != before || band.data != factors ||
      !Succeeded(asc::Pbtrs(provider, band.ConstView(), rhs.View(), plan,
                            workspace, report),
                 report) ||
      band.data != factors || !rhs.PaddingEquals(before)) {
    return false;
  }
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < 2; ++j) {
      Wide product{};
      for (std::size_t k = 0; k < N; ++k) {
        product += Original<T>(i, k) * Widen(rhs.At(k, j));
      }
      if (!Near<T>(product, Widen(before[rhs.Offset(i, j)]), 40) ||
          !Near<T>(Widen(rhs.At(i, j)), Widen(Value<T>(1 + (i % 3) + j, 0.25)),
                   8)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T, std::size_t N, std::size_t Kd>
bool Exercise(const asc::ReferenceLapackProvider& provider,
              asc::DenseBlasLayout layout, asc::DenseBlasTriangle triangle,
              bool blocked) {
  Band<T, N, Kd> band{{}, layout, triangle};
  band.data.fill(Value<T>(-91, 23));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if (band.Selected(i, j)) {
        const auto value = Original<T>(i, j);
        band.At(i, j) = Value<T>(value.real(), i == j ? 77 : value.imag());
      }
    }
  }
  const auto before = band.data;
  const auto plan =
      Take(blocked ? asc::QueryPbtrfWorkspace(provider, band.View())
                   : asc::QueryPbtf2Workspace(provider, band.View()));
  if (band.data != before ||
      plan.regions[kPacking].minimum_entries !=
          static_cast<asc::extent_t>(layout == kRow ? N * (Kd + 1) : 0)) {
    return false;
  }
  std::array<T, N*(Kd + 1) + 2 * N + 2> packing{};
  packing.fill(Value<T>(-97, 31));
  asc::LapackWorkspace workspace;
  workspace.regions[kPacking] = {packing.data() + 1,
                                 (packing.size() - 2) * sizeof(T), kHost};
  asc::LapackReport report;
  const auto status =
      blocked ? asc::Pbtrf(provider, band.View(), plan, workspace, report)
              : asc::Pbtf2(provider, band.View(), plan, workspace, report);
  if (!Succeeded(status, report) || !band.PaddingEquals(before) ||
      !Reconstruct(band)) {
    return false;
  }
  // The same factors solve independently laid-out RHS without refactorization.
  return Solve(provider, band, kColumn, workspace) &&
         Solve(provider, band, kRow, workspace) &&
         packing.front() == Value<T>(-97, 31) &&
         packing.back() == Value<T>(-97, 31);
}

template <typename T, std::size_t N, std::size_t Kd>
bool Partial(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasLayout layout, asc::DenseBlasTriangle triangle,
             bool blocked, std::size_t failed, std::size_t normalized) {
  Band<T, N, Kd> band{{}, layout, triangle};
  band.data.fill(Value<T>(-91, 23));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if (band.Selected(i, j)) {
        band.At(i, j) = T{};
        if (i == j) {
          band.At(i, j) = Value<T>(i == failed ? -1 : 4, 77);
        }
      }
    }
  }
  const auto before = band.data;
  const auto plan =
      Take(blocked ? asc::QueryPbtrfWorkspace(provider, band.View())
                   : asc::QueryPbtf2Workspace(provider, band.View()));
  std::array<T, N*(Kd + 1)> packing{};
  asc::LapackWorkspace workspace;
  workspace.regions[kPacking] = {packing.data(), sizeof(packing), kHost};
  asc::LapackReport report;
  const auto status =
      blocked ? asc::Pbtrf(provider, band.View(), plan, workspace, report)
              : asc::Pbtf2(provider, band.View(), plan, workspace, report);
  if (status.code() != asc::ErrorCode::kNumerical || !report.called_provider ||
      report.native_info != static_cast<asc::index_t>(failed + 1) ||
      report.diagnostic_index != static_cast<asc::index_t>(failed) ||
      report.outcome != asc::LapackOutcome::kNotPositiveDefinite ||
      report.output_validity != asc::LapackOutputValidity::kDocumentedPartial ||
      !band.PaddingEquals(before)) {
    return false;
  }
  for (std::size_t i = 0; i < N; ++i) {
    double diagonal = i == failed ? -1 : 4;
    if (i < failed) {
      diagonal = 2;
    }
    const auto expected = Value<T>(diagonal, i < normalized ? 0 : 77);
    if (band.At(i, i) != expected) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kRow, kColumn}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const bool blocked : {false, true}) {
        if (!Exercise<T, 0, 0>(provider, layout, triangle, blocked) ||
            !Exercise<T, 1, 0>(provider, layout, triangle, blocked) ||
            !Exercise<T, 4, 1>(provider, layout, triangle, blocked) ||
            !Exercise<T, 96, 65>(provider, layout, triangle, blocked)) {
          return false;
        }
      }
      // One-based failure4: prior CHER reaches diagonal4 but not5.
      // Blocked failure33: the completed first32 columns' HERK reaches97.
      if (!Partial<T, 7, 1>(provider, layout, triangle, false, 3, 4) ||
          !Partial<T, 128, 65>(provider, layout, triangle, true, 32, 97)) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (!Run<float>(provider) || !Run<double>(provider) ||
      !Run<std::complex<float>>(provider) ||
      !Run<std::complex<double>>(provider)) {
    std::fprintf(stderr, "Installed band reconstruction/solve check failed\n");
    return 1;
  }
  return 0;
}
