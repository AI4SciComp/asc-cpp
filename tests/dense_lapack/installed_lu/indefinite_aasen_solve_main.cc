#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen.h"
#include "asc/dense/providers/lapack_indefinite_aasen_solve.h"
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
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrfAaWorkspace(provider, triangle, matrix, pivots);
    }
  }
  return asc::QuerySytrfAaWorkspace(provider, triangle, matrix, pivots);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool he,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrfAa(provider, triangle, matrix, pivots, plan, workspace,
                          report);
    }
  }
  return asc::SytrfAa(provider, triangle, matrix, pivots, plan, workspace,
                      report);
}
template <typename T>
struct Scratch {
  std::array<T, 4357> scalar{};
  std::array<T, 4750> packing{};
  alignas(16) std::array<std::byte, 600> integers{};
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
auto QuerySolve(const asc::ReferenceLapackProvider& provider, bool he,
                asc::DenseBlasTriangle tri, asc::DenseBlasMatrixView<const T> a,
                asc::RawLapackPivotView p, asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrsAaWorkspace(provider, tri, a, p, b);
    }
  }
  return asc::QuerySytrsAaWorkspace(provider, tri, a, p, b);
}
template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider, bool he,
                  asc::DenseBlasTriangle tri,
                  asc::DenseBlasMatrixView<const T> a,
                  asc::RawLapackPivotView p, asc::DenseBlasMatrixView<T> b,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& work, asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrsAa(provider, tri, a, p, b, plan, work, report);
    }
  }
  return asc::SytrsAa(provider, tri, a, p, b, plan, work, report);
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
template <typename T, std::size_t N, std::size_t Rhs>
bool Case(const asc::ReferenceLapackProvider& provider, bool he,
          asc::DenseBlasTriangle tri, asc::DenseBlasLayout al,
          asc::DenseBlasLayout bl, int mode) {
  using installed_internal::Wide;
  using installed_internal::Widen;
  installed_internal::Matrix<T, N, N> a{{}, al};
  installed_internal::Matrix<T, N, Rhs> b{{}, bl};
  std::array<T, N * N> full{};
  std::array<T, N * Rhs> solution{};
  Initialize(a, b, he, tri, full, solution);
  const auto before_b = b.data;
  std::array<asc::index_t, N + 2> pivots;
  pivots.fill(-67);
  const auto mutable_p = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto fp = Take(QueryFactor(provider, tri, he, a.View(), mutable_p));
  Scratch<T> factor_scratch(fp, mode, N);
  asc::LapackReport report;
  bool pass = Check(Factor(provider, tri, he, a.View(), mutable_p, fp,
                           factor_scratch.workspace, report)
                        .ok(),
                    "checked AA producer");
  const auto before_a = a.data;
  const auto before_p = pivots;
  const auto p = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, N, asc::LapackFactorFamily::kAasen,
      {pivots.data(), sizeof(pivots), kHost}));
  const auto plan =
      Take(QuerySolve(provider, he, tri, a.ConstView(), p, b.View()));
  Scratch<T> scratch(plan, mode, N);
  pass = Check(Solve(provider, he, tri, a.ConstView(), p, b.View(), plan,
                     scratch.workspace, report)
                   .ok(),
               "checked AA solve") &&
         pass;
  constexpr bool kActive = N != 0 && Rhs != 0;
  pass = Check(report.called_provider == kActive &&
                   report.native_info.has_value() == kActive &&
                   report.output_validity ==
                       asc::LapackOutputValidity::kComplete &&
                   report.factor_family == asc::LapackFactorFamily::kAasen,
               "AA report") &&
         pass;
  if constexpr (kActive) {
    pass = Check(report.native_info.value_or(-1) == 0, "native INFO") && pass;
  }
  pass = VerifySolution(b, before_b, full, solution) && pass;
  pass = Check(EqualBytes(a.data, before_a) && pivots == before_p &&
                   b.PaddingEquals(before_b),
               "immutable factor/pivots and RHS guards") &&
         pass;
  return scratch.Guards() && factor_scratch.Guards() && pass;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool he) {
  bool pass = true;
  int cases = 0;
  for (auto tri : {kUpper, kLower}) {
    for (auto al : {kColumn, kRow}) {
      for (auto bl : {kColumn, kRow}) {
        for (int mode : {0, 2}) {
          pass = Case<T, 0, 0>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 0, 3>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 1, 0>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 1, 3>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 3, 0>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 3, 3>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 67, 0>(provider, he, tri, al, bl, mode) && pass;
          pass = Case<T, 67, 3>(provider, he, tri, al, bl, mode) && pass;
          cases += 8;
        }
      }
    }
  }
  std::printf("Installed AA solve cases=%d pass=%d\n", cases, pass);
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
