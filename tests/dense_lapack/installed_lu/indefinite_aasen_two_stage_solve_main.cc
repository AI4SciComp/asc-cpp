#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage_solve.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
namespace {
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kHost = installed_internal::kHost;
constexpr auto kColumn = installed_internal::kColumn;
constexpr auto kRow = installed_internal::kRow;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
bool Check(bool pass, const char* message) {
  if (!pass) {
    std::fprintf(stderr, "check failed: %s\n", message);
  }
  return pass;
}
// Ignored storage must preserve its bytes, including NaN/sign payloads.
// Compare byte spans explicitly; this is not floating-point value equality.
template <typename T>
bool EqualBytes(const T& actual, const T& expected) {
  const auto a = std::as_bytes(std::span{&actual, 1});
  const auto b = std::as_bytes(std::span{&expected, 1});
  return std::equal(a.begin(), a.end(), b.begin());
}
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool he,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<T> band,
                 asc::DenseBlasVectorView<asc::index_t> pivots,
                 asc::DenseBlasVectorView<asc::index_t> band_pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrfAa2StageWorkspace(provider, triangle, matrix, band,
                                              pivots, band_pivots);
    }
  }
  return asc::QuerySytrfAa2StageWorkspace(provider, triangle, matrix, band,
                                          pivots, band_pivots);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool he,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> band,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasVectorView<asc::index_t> band_pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrfAa2Stage(provider, triangle, matrix, band, pivots,
                                band_pivots, plan, workspace, report);
    }
  }
  return asc::SytrfAa2Stage(provider, triangle, matrix, band, pivots,
                            band_pivots, plan, workspace, report);
}
template <typename T>
struct Scratch {
  std::array<T, 12866> scalar{};
  std::array<T, 4750> packing{};
  alignas(16) std::array<std::byte, 1120> integers{};
  asc::LapackWorkspace workspace;
  Scratch(const asc::LapackWorkspacePlan& plan, int mode, asc::extent_t n) {
    scalar.fill(T{-71});
    packing.fill(T{-73});
    integers.fill(std::byte{0x59});
    for (std::size_t kind = 0; kind < plan.regions.size(); ++kind) {
      const auto& region = plan.regions[kind];
      auto entries = region.preferred_entries;
      if (kind == static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)) {
        if (mode == 0) {
          entries = region.minimum_entries;
        } else if (mode == 1) {
          entries = std::min(entries, std::max(region.minimum_entries, 3 * n));
        }
      }
      if (entries == 0) {
        continue;
      }
      const auto bytes = static_cast<std::size_t>(entries) * region.entry_bytes;
      if (kind == static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar)) {
        workspace.regions[kind] = {scalar.data() + 1, bytes, kHost};
      } else if (kind == static_cast<std::size_t>(
                             asc::LapackWorkspaceKind::kLayoutConversion)) {
        workspace.regions[kind] = {packing.data() + 1, bytes, kHost};
      } else if (kind ==
                 static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger)) {
        workspace.regions[kind] = {integers.data() + 16, bytes, kHost};
      }
    }
  }
  [[nodiscard]] bool Guards() const {
    const auto scalar_bytes = workspace
                                  .regions[static_cast<std::size_t>(
                                      asc::LapackWorkspaceKind::kScalar)]
                                  .size();
    const auto packing_bytes =
        workspace
            .regions[static_cast<std::size_t>(
                asc::LapackWorkspaceKind::kLayoutConversion)]
            .size();
    const auto integer_bytes = workspace
                                   .regions[static_cast<std::size_t>(
                                       asc::LapackWorkspaceKind::kInteger)]
                                   .size();
    bool pass = scalar.front() == T{-71} && packing.front() == T{-73};
    for (std::size_t i = 1 + scalar_bytes / sizeof(T); i < scalar.size(); ++i) {
      pass = (scalar[i] == T{-71}) && pass;
    }
    for (std::size_t i = 1 + packing_bytes / sizeof(T); i < packing.size();
         ++i) {
      pass = (packing[i] == T{-73}) && pass;
    }
    for (std::size_t i = 0; i < integers.size(); ++i) {
      if (i < 16 || i >= 16 + integer_bytes) {
        pass = (integers[i] == std::byte{0x59}) && pass;
      }
    }
    return Check(pass, "caller workspace guards");
  }
};
template <typename T>
auto QuerySolve(const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasTriangle tri, bool he,
                asc::DenseBlasMatrixView<const T> a,
                asc::DenseBlasVectorView<const T> tb, asc::RawLapackPivotView p,
                asc::DenseBlasVectorView<const asc::index_t> q,
                asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrsAa2StageWorkspace(provider, tri, a, tb, p, q, b);
    }
  }
  return asc::QuerySytrsAa2StageWorkspace(provider, tri, a, tb, p, q, b);
}
template <typename T>
asc::Status Solve(
    const asc::ReferenceLapackProvider& provider, asc::DenseBlasTriangle tri,
    bool he, asc::DenseBlasMatrixView<const T> a,
    asc::DenseBlasVectorView<const T> tb, asc::RawLapackPivotView p,
    asc::DenseBlasVectorView<const asc::index_t> q,
    asc::DenseBlasMatrixView<T> b, const asc::LapackWorkspacePlan& plan,
    const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrsAa2Stage(provider, tri, a, tb, p, q, b, plan, workspace,
                                report);
    }
  }
  return asc::SytrsAa2Stage(provider, tri, a, tb, p, q, b, plan, workspace,
                            report);
}
template <typename T, std::size_t N, std::size_t Rhs>
void Initialize(installed_internal::Matrix<T, N, N>& a,
                installed_internal::Matrix<T, N, Rhs>& b, bool he,
                asc::DenseBlasTriangle tri, std::array<T, N * N>& full,
                std::array<T, N * Rhs>& solution) {
  using installed_internal::Wide;
  using installed_internal::Widen;
  a.data.fill(Value<T>(-61, 17));
  b.data.fill(Value<T>(-63, 19));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      T value = i == j ? Value<T>(4, he ? 0 : 0.25) : T{};
      if (N >= 3 && j == 0) {
        if (i == 0) {
          value = T{};
        }
        if (i == 1) {
          value = Value<T>(1, 0.125);
        }
        if (i == 2) {
          value = Value<T>(2, 0.25);
        }
      }
      full[i * N + j] = value;
      full[j * N + i] = he ? installed_internal::Conjugate(value) : value;
    }
  }
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if (tri == kUpper ? i <= j : i >= j) {
        a.At(i, j) = full[i * N + j];
      }
    }
    for (std::size_t j = 0; j < Rhs; ++j) {
      solution[j * N + i] =
          Value<T>(static_cast<double>((i + j) % 3 + 1) / 4, 0.125);
    }
  }
  for (std::size_t j = 0; j < Rhs; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      Wide sum{};
      for (std::size_t k = 0; k < N; ++k) {
        sum += Widen(full[i * N + k]) * Widen(solution[j * N + k]);
      }
      using Real = asc::DenseBlasRealType<T>;
      if constexpr (asc::DenseBlasComplex<T>) {
        b.At(i, j) =
            T{static_cast<Real>(sum.real()), static_cast<Real>(sum.imag())};
      } else {
        b.At(i, j) = static_cast<T>(sum.real());
      }
    }
  }
}
template <typename T, std::size_t N, std::size_t Rhs>
bool VerifySolution(const installed_internal::Matrix<T, N, Rhs>& b,
                    const std::array<T, (N + 1) * (Rhs + 1)>& before_b,
                    const std::array<T, N * N>& full,
                    const std::array<T, N * Rhs>& solution) {
  using installed_internal::Wide;
  using installed_internal::Widen;
  bool pass = true;
  long double error = 0;
  long double residual = 0;
  long double norm_a = 0;
  long double norm_x = 0;
  long double norm_b = 0;
  for (std::size_t j = 0; j < Rhs; ++j) {
    for (std::size_t i = 0; i < N; ++i) {
      const auto x = Widen(b.At(i, j));
      pass = Check(std::isfinite(x.real()) && std::isfinite(x.imag()),
                   "finite solution") &&
             pass;
      error = std::max(error, std::abs(x - Widen(solution[j * N + i])));
      norm_x = std::max(norm_x, std::abs(Widen(solution[j * N + i])));
      Wide product{};
      long double row_norm = 0;
      for (std::size_t k = 0; k < N; ++k) {
        product += Widen(full[i * N + k]) * Widen(b.At(k, j));
        row_norm += std::abs(Widen(full[i * N + k]));
      }
      norm_a = std::max(norm_a, row_norm);
      const auto rhs = Widen(before_b[b.Offset(i, j)]);
      norm_b = std::max(norm_b, std::abs(rhs));
      residual = std::max(residual, std::abs(product - rhs));
    }
  }
  const auto tolerance =
      64 * std::max(N, std::size_t{1}) *
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  pass = Check(error <= tolerance * norm_x &&
                   residual <= tolerance * (norm_a * norm_x + norm_b),
               "known solution and independent residual") &&
         pass;
  return pass;
}
template <std::size_t N, std::size_t Rhs, bool Singular>
bool CheckReport(const asc::Status& status, const asc::LapackReport& report) {
  constexpr bool kActive = N != 0 && Rhs != 0;
  constexpr bool kReject = kActive && Singular;
  bool pass = true;
  pass =
      Check(kReject ? status.code() == asc::ErrorCode::kNumerical : status.ok(),
            "checked two-stage solve") &&
      pass;
  pass = Check(report.called_provider == (kActive && !kReject) &&
                   report.native_info.has_value() == (kActive && !kReject) &&
                   report.output_validity ==
                       (kReject ? asc::LapackOutputValidity::kUnchanged
                                : asc::LapackOutputValidity::kComplete) &&
                   report.factor_family == asc::LapackFactorFamily::kAasen,
               "two-stage solve report") &&
         pass;
  if constexpr (kReject) {
    pass = Check(report.outcome == asc::LapackOutcome::kSingular &&
                     report.diagnostic_index == N - 1,
                 "zero band U diagonal preflight") &&
           pass;
  } else if constexpr (kActive) {
    pass = Check(report.native_info.value_or(-1) == 0, "native INFO") && pass;
  }
  return pass;
}
template <typename T, std::size_t N, std::size_t Rhs, bool Singular>
bool SolveRepeated(const asc::ReferenceLapackProvider& provider, bool he,
                   asc::DenseBlasTriangle tri,
                   installed_internal::Matrix<T, N, N>& a,
                   installed_internal::Matrix<T, N, Rhs>& b,
                   const std::array<T, N * N>& full,
                   std::array<T, N * Rhs>& solution,
                   std::array<asc::index_t, N + 2>& pivots,
                   std::array<asc::index_t, N + 2>& band_pivots,
                   std::vector<T>& tb, int work_mode) {
  const auto initial_b = b.data;
  const auto initial_solution = solution;
  asc::LapackReport report;
  bool pass = true;
  const auto before_a = a.data;
  const auto before_p = pivots;
  const auto before_q = band_pivots;
  const auto before_tb = tb;
  const auto p = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, N, asc::LapackFactorFamily::kAasen,
      {pivots.data(), sizeof(pivots), kHost}));
  const auto q = Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
      band_pivots.data() + 1, N, 1,
      {band_pivots.data(), sizeof(band_pivots), kHost}));
  const auto band = Take(asc::DenseBlasVectorView<const T>::Create(
      tb.data() + 1, tb.size() - 2, 1,
      {tb.data(), tb.size() * sizeof(T), kHost}));
  const auto plan =
      Take(QuerySolve(provider, tri, he, a.ConstView(), band, p, q, b.View()));
  Scratch<T> scratch(plan, work_mode, N);
  for (int repeat = 1; repeat <= 2; ++repeat) {
    b.data = initial_b;
    for (std::size_t j = 0; j < Rhs; ++j) {
      for (std::size_t i = 0; i < N; ++i) {
        b.At(i, j) *= Value<T>(repeat);
        solution[j * N + i] = initial_solution[j * N + i] * Value<T>(repeat);
      }
    }
    const auto before_b = b.data;
    const auto before_integers = scratch.integers;
    const auto before_packing = scratch.packing;
    const auto before_scalar = scratch.scalar;
    const auto status = Solve(provider, tri, he, a.ConstView(), band, p, q,
                              b.View(), plan, scratch.workspace, report);
    constexpr bool kActive = N != 0 && Rhs != 0;
    constexpr bool kReject = kActive && Singular;
    pass = CheckReport<N, Rhs, Singular>(status, report) && pass;
    if constexpr (kActive && !kReject) {
      pass = VerifySolution(b, before_b, full, solution) && pass;
    }
    if constexpr (!kActive || kReject) {
      pass = Check(EqualBytes(b.data, before_b) &&
                       scratch.integers == before_integers &&
                       EqualBytes(scratch.packing, before_packing) &&
                       EqualBytes(scratch.scalar, before_scalar),
                   "noncall preserves RHS and workspace") &&
             pass;
    }
    pass = Check(EqualBytes(a.data, before_a) && pivots == before_p &&
                     band_pivots == before_q &&
                     std::equal(std::as_bytes(std::span{tb}).begin(),
                                std::as_bytes(std::span{tb}).end(),
                                std::as_bytes(std::span{before_tb}).begin()) &&
                     b.PaddingEquals(before_b),
                 "immutable A/TB/P/Q and RHS guards") &&
           pass;
    pass = scratch.Guards() && pass;
  }
  return pass;
}
template <typename T, std::size_t N, std::size_t Rhs, bool Singular = false>
bool Case(const asc::ReferenceLapackProvider& provider, bool he,
          asc::DenseBlasTriangle tri, asc::DenseBlasLayout al,
          asc::DenseBlasLayout bl, int band_mode, int work_mode) {
  installed_internal::Matrix<T, N, N> a{{}, al};
  installed_internal::Matrix<T, N, Rhs> b{{}, bl};
  std::array<T, N * N> full{};
  std::array<T, N * Rhs> solution{};
  Initialize(a, b, he, tri, full, solution);
  if constexpr (Singular) {
    static_assert(N > 1);
    for (std::size_t i = 0; i < N; ++i) {
      if (tri == kLower) {
        a.At(N - 1, i) = T{};
      } else {
        a.At(i, N - 1) = T{};
      }
    }
  }
  const std::size_t ldtb = band_mode == 0 ? 4 : 577;
  std::vector<T> tb(N * ldtb + 2, T{-79});
  std::array<asc::index_t, N + 2> pivots;
  std::array<asc::index_t, N + 2> band_pivots;
  pivots.fill(-67);
  band_pivots.fill(-67);
  const auto mutable_p = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto mutable_q = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      band_pivots.data() + 1, N, 1,
      {band_pivots.data(), sizeof(band_pivots), kHost}));
  const auto mutable_tb = Take(asc::DenseBlasVectorView<T>::Create(
      tb.data() + 1, N * ldtb, 1, {tb.data(), tb.size() * sizeof(T), kHost}));
  const auto fp = Take(QueryFactor(provider, tri, he, a.View(), mutable_tb,
                                   mutable_p, mutable_q));
  Scratch<T> factor_scratch(fp, work_mode, N);
  asc::LapackReport report;
  const auto factor_status =
      Factor(provider, tri, he, a.View(), mutable_tb, mutable_p, mutable_q, fp,
             factor_scratch.workspace, report);
  bool pass =
      Check(Singular ? factor_status.code() == asc::ErrorCode::kNumerical
                     : factor_status.ok(),
            "checked two-stage producer");
  if (!pass) {
    return false;
  }
  return SolveRepeated<T, N, Rhs, Singular>(provider, he, tri, a, b, full,
                                            solution, pivots, band_pivots, tb,
                                            work_mode) &&
         factor_scratch.Guards();
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool he) {
  bool pass = true;
  int cases = 0;
  for (const auto tri : {kUpper, kLower}) {
    for (const auto al : {kColumn, kRow}) {
      for (const auto bl : {kColumn, kRow}) {
        for (const int band : {0, 2}) {
          for (const int work : {0, 2}) {
            pass = Case<T, 0, 0>(provider, he, tri, al, bl, band, work) && pass;
            pass = Case<T, 0, 3>(provider, he, tri, al, bl, band, work) && pass;
            pass = Case<T, 1, 0>(provider, he, tri, al, bl, band, work) && pass;
            pass = Case<T, 1, 3>(provider, he, tri, al, bl, band, work) && pass;
            pass = Case<T, 3, 0>(provider, he, tri, al, bl, band, work) && pass;
            pass = Case<T, 3, 3>(provider, he, tri, al, bl, band, work) && pass;
            pass =
                Case<T, 67, 0>(provider, he, tri, al, bl, band, work) && pass;
            pass =
                Case<T, 67, 3>(provider, he, tri, al, bl, band, work) && pass;
            pass = Case<T, 3, 0, true>(provider, he, tri, al, bl, band, work) &&
                   pass;
            pass = Case<T, 3, 3, true>(provider, he, tri, al, bl, band, work) &&
                   pass;
            cases += 10;
          }
        }
      }
    }
  }
  std::printf("Installed two-stage Aasen solve cases=%d repeats=2 pass=%d\n",
              cases, pass);
  return pass;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  bool pass = Run<float>(provider, false);
  pass = Run<double>(provider, false) && pass;
  pass = Run<std::complex<float>>(provider, false) && pass;
  pass = Run<std::complex<double>>(provider, false) && pass;
  pass = Run<std::complex<float>>(provider, true) && pass;
  pass = Run<std::complex<double>>(provider, true) && pass;
  return pass ? 0 : 1;
}
