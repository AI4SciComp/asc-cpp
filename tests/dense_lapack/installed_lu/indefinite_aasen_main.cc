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
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen.h"
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
auto Query(const asc::ReferenceLapackProvider& provider,
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
  std::array<T, 4491> packing{};
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
template <typename T, std::size_t N>
void Initialize(installed_internal::Matrix<T, N, N>& a, bool he,
                asc::DenseBlasTriangle tri) {
  a.data.fill(Value<T>(-61, 17));
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = 0; j < N; ++j) {
      if (tri == kUpper ? i <= j : i >= j) {
        T value{};
        if (i == j && (N == 1 || i >= 3)) {
          value = T{4};
        } else if (i + j == 1) {
          value = Value<T>(1, 0.5);
        } else if ((i == 0 && j == 2) || (i == 2 && j == 0)) {
          value = Value<T>(2, 1);
        }
        if (he && i > j) {
          value = installed_internal::Conjugate(value);
        }
        if constexpr (asc::DenseBlasComplex<T>) {
          if (he && i == j) {
            value.imag(
                std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
          }
        }
        a.At(i, j) = value;
      }
    }
  }
}
template <typename T, std::size_t N>
bool Verify(const installed_internal::Matrix<T, N, N>& a,
            const std::array<T, (N + 1) * (N + 1)>& before,
            const std::array<asc::index_t, N + 2>& pivots, bool he,
            asc::DenseBlasTriangle tri) {
  bool pass = true;
  for (std::size_t i = 0; i < N; ++i) {
    const asc::index_t target =
        N >= 3 && i == 1 ? 3 : static_cast<asc::index_t>(i) + 1;
    pass = Check(pivots[i + 1] == target, "Aasen known positive permutation") &&
           pass;
    for (std::size_t j = 0; j < N; ++j) {
      if (tri == kUpper ? i <= j : i >= j) {
        T expected{};
        if (i == j && (N == 1 || i >= 3)) {
          expected = T{4};
        } else if (i + j == 1) {
          expected = Value<T>(2, 1);
          if (he && i > j) {
            expected = installed_internal::Conjugate(expected);
          }
        } else if ((i == 0 && j == 2) || (i == 2 && j == 0)) {
          expected = T{0.5};
        }
        const auto error = std::abs(installed_internal::Widen(a.At(i, j)) -
                                    installed_internal::Widen(expected));
        pass =
            Check(std::isfinite(error) &&
                      error <= 32 * std::numeric_limits<
                                        asc::DenseBlasRealType<T>>::epsilon(),
                  "Aasen analytic T and shifted multipliers") &&
            pass;
      } else {
        pass = Check(EqualBytes(a.At(i, j), before[a.Offset(i, j)]),
                     "ignored triangle bytes") &&
               pass;
      }
    }
  }
  pass = Check(a.PaddingEquals(before), "matrix padding") && pass;
  return Check(pivots.front() == -67 && pivots.back() == -67, "pivot guards") &&
         pass;
}
template <typename T, std::size_t N>
bool Case(const asc::ReferenceLapackProvider& provider, bool he,
          asc::DenseBlasTriangle tri, asc::DenseBlasLayout layout, int mode) {
  installed_internal::Matrix<T, N, N> a{{}, layout};
  Initialize(a, he, tri);
  const auto before = a.data;
  std::array<asc::index_t, N + 2> pivots;
  pivots.fill(-67);
  const auto p = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, N, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto plan = Take(Query(provider, tri, he, a.View(), p));
  Scratch<T> scratch(plan, mode, N);
  asc::LapackReport report;
  const auto status =
      Factor(provider, tri, he, a.View(), p, plan, scratch.workspace, report);
  bool pass = Check(
      status.ok() && report.factor_family == asc::LapackFactorFamily::kAasen &&
          report.output_validity == asc::LapackOutputValidity::kComplete &&
          report.called_provider == (N != 0) &&
          report.native_info.has_value() == (N != 0),
      "Aasen report");
  pass = Verify(a, before, pivots, he, tri) && pass;
  return scratch.Guards() && pass;
}
template <typename T>
bool Run(const asc::ReferenceLapackProvider& provider, bool he, int& cases) {
  bool pass = true;
  for (const auto tri : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (int mode : {0, 1, 2}) {
        pass = Case<T, 0>(provider, he, tri, layout, mode) && pass;
        pass = Case<T, 1>(provider, he, tri, layout, mode) && pass;
        pass = Case<T, 3>(provider, he, tri, layout, mode) && pass;
        pass = Case<T, 67>(provider, he, tri, layout, mode) && pass;
        cases += 4;
      }
    }
  }
  return pass;
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  int cases = 0;
  bool pass = true;
  pass = Run<float>(provider, false, cases) && pass;
  pass = Run<double>(provider, false, cases) && pass;
  pass = Run<std::complex<float>>(provider, false, cases) && pass;
  pass = Run<std::complex<double>>(provider, false, cases) && pass;
  pass = Run<std::complex<float>>(provider, true, cases) && pass;
  pass = Run<std::complex<double>>(provider, true, cases) && pass;
  std::printf("Installed single-stage Aasen cases=%d passed=%d\n", cases,
              static_cast<int>(pass));
  return pass ? 0 : 1;
}
