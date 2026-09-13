#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed.h"
#include "asc/dense/providers/lapack_indefinite_packed_inverse.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Value;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);

struct Checks {
  int workflows = 0;
  int failures = 0;
  void Expect(bool value, const char* message) {
    if (!value) {
      if (failures < 20) {
        std::fprintf(stderr, "Packed inverse consumer: %s (workflow %d)\n",
                     message, workflows);
      }
      ++failures;
    }
  }
};

// Enumerate the public physical storage without adapter indexing helpers.
template <typename Visit>
void Slots(asc::extent_t n, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, const Visit& visit) {
  std::size_t slot = 0;
  for (asc::extent_t major = 0; major < n; ++major) {
    for (asc::extent_t minor = 0; minor < n; ++minor) {
      const auto i = layout == kColumn ? minor : major;
      const auto j = layout == kColumn ? major : minor;
      if (triangle == kUpper ? i <= j : i >= j) {
        visit(slot++, i, j);
      }
    }
  }
}

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasPackedMatrixView<T> matrix,
           asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHptrfWorkspace(provider, triangle, matrix, pivots);
    }
  }
  return asc::QuerySptrfWorkspace(provider, triangle, matrix, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasPackedMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hptrf(provider, triangle, matrix, pivots, plan, workspace,
                        report);
    }
  }
  return asc::Sptrf(provider, triangle, matrix, pivots, plan, workspace,
                    report);
}

template <typename T>
std::array<T, 18> Initialize(std::array<T, 18>& data, asc::extent_t n,
                             asc::DenseBlasTriangle triangle,
                             asc::DenseBlasLayout layout, bool hermitian,
                             int exponent, bool singular) {
  const auto scale = std::ldexp(1.0, exponent);
  // A block diagonal D with zero-diagonal nonsingular 2x2 blocks forces
  // genuine paired pivots. There are no triangular multipliers: A = D.
  // Check every factor slot against this independent matrix equation.
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    T value{};
    if (!singular) {
      if (i == j && (n == 1 || i == 2)) {
        value = Value<T>(3 * scale, hermitian ? 0 : scale / 4);
      } else if ((i == 0 && j == 1) || (i == 1 && j == 0) ||
                 (i == 3 && j == 4) || (i == 4 && j == 3)) {
        value = Value<T>(scale, scale / 4);
        if (hermitian && i < j) {
          value = installed_internal::Conjugate(value);
        }
      }
    }
    data[slot + 1] = value;
  });
  const auto expected = data;
  // The Hermitian API ignores the input imaginary diagonal, even a NaN.
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
        if (i == j) {
          data[slot + 1].imag(
              std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN());
        }
      });
    }
  }
  return expected;
}

void CheckReport(Checks& checks, asc::extent_t n,
                 asc::DenseBlasTriangle triangle, bool singular,
                 const asc::Status& status, const asc::LapackReport& report) {
  checks.Expect(status.ok() == (n == 0 || !singular), "completion status");
  checks.Expect(report.called_provider == (n != 0), "provider call boundary");
  checks.Expect(
      report.output_validity ==
          (n != 0 && singular ? asc::LapackOutputValidity::kDocumentedPartial
                              : asc::LapackOutputValidity::kComplete),
      "factor validity");
  if (n == 0) {
    checks.Expect(!report.native_info, "empty call has no native INFO");
  } else if (singular) {
    const auto info = triangle == kUpper ? n : 1;
    checks.Expect(status.code() == asc::ErrorCode::kNumerical &&
                      report.outcome == asc::LapackOutcome::kSingular &&
                      report.native_info == info &&
                      report.diagnostic_index == info - 1,
                  "singular report and traversal");
  } else {
    checks.Expect(installed_internal::Succeeded(status, report),
                  "successful native completion");
  }
}

template <typename T>
auto QueryInverse(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, bool hermitian,
                  asc::DenseBlasPackedMatrixView<T> factors,
                  asc::RawLapackPivotView pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHptriWorkspace(provider, triangle, factors, pivots);
    }
  }
  return asc::QuerySptriWorkspace(provider, triangle, factors, pivots);
}

template <typename T>
asc::Status Inverse(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    asc::DenseBlasPackedMatrixView<T> factors,
                    asc::RawLapackPivotView pivots,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hptri(provider, triangle, factors, pivots, plan, workspace,
                        report);
    }
  }
  return asc::Sptri(provider, triangle, factors, pivots, plan, workspace,
                    report);
}

template <typename T>
void CheckInverse(Checks& checks, asc::extent_t n,
                  asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                  bool hermitian, const std::array<T, 18>& original,
                  const std::array<T, 18>& inverse) {
  using installed_internal::Wide;
  using installed_internal::Widen;
  const long double epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  std::array<Wide, 25> a{};
  std::array<Wide, 25> b{};
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    const auto value = Widen(inverse[slot + 1]);
    const auto source = Widen(original[slot + 1]);
    const auto expected =
        source == Wide{} ? Wide{}
                         : Wide{1} / (hermitian ? std::conj(source) : source);
    checks.Expect(
        std::isfinite(value.real()) && std::isfinite(value.imag()) &&
            std::abs(value - expected) <= 64 * epsilon * std::abs(expected),
        "independent block reciprocal");
    a[i * n + j] = source;
    a[j * n + i] = hermitian ? std::conj(source) : source;
    b[i * n + j] = value;
    b[j * n + i] = hermitian ? std::conj(value) : value;
  });
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      Wide left{};
      Wide right{};
      for (asc::extent_t k = 0; k < n; ++k) {
        left += a[i * n + k] * b[k * n + j];
        right += b[i * n + k] * a[k * n + j];
      }
      const Wide identity{i == j ? 1.0L : 0.0L};
      checks.Expect(std::abs(left - identity) <=
                            128 * epsilon * std::max(asc::extent_t{1}, n) &&
                        std::abs(right - identity) <=
                            128 * epsilon * std::max(asc::extent_t{1}, n),
                    "both inverse residuals");
    }
  }
}

template <typename T>
void InvertWorkflow(Checks& checks,
                    const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    bool singular, asc::DenseBlasPackedMatrixView<T> factors,
                    const std::array<asc::index_t, 7>& pivots) {
  constexpr auto kScalar =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
  const auto n = factors.order();
  const auto raw = Take(asc::RawLapackPivotView::Create(
      pivots.data() + 1, n, asc::LapackFactorFamily::kBunchKaufman,
      {pivots.data(), sizeof(pivots), kHost}));
  const auto plan =
      Take(QueryInverse(provider, triangle, hermitian, factors, raw));
  checks.Expect(plan.regions[kScalar].minimum_entries == n &&
                    plan.regions[kInteger].minimum_entries == n,
                "exact N scalar WORK and N native INTEGER entries");
  std::array<T, 7> scalar;
  std::array<T, 20> packing;
  alignas(std::max_align_t) std::array<std::byte, 96> integers;
  scalar.fill(T{-83});
  packing.fill(T{-89});
  integers.fill(std::byte{0x61});
  const auto packed_count =
      static_cast<std::size_t>(plan.regions[kPacking].minimum_entries);
  const auto integer_bytes =
      static_cast<std::size_t>(n) * plan.regions[kInteger].entry_bytes;
  asc::LapackWorkspace workspace;
  if (n != 0) {
    workspace.regions[kScalar] = {
        scalar.data() + 1, static_cast<std::size_t>(n) * sizeof(T), kHost};
    workspace.regions[kInteger] = {integers.data() + 16, integer_bytes, kHost};
    workspace.regions[kPacking] = {packing.data() + 1, packed_count * sizeof(T),
                                   kHost};
  }
  const auto before_pivots = pivots;
  asc::LapackReport report;
  const auto status = Inverse(provider, triangle, hermitian, factors, raw, plan,
                              workspace, report);
  CheckReport(checks, n, triangle, singular, status, report);
  checks.Expect(!report.factor_family,
                "inverse does not issue a factor certificate");
  checks.Expect(pivots == before_pivots, "immutable input pivots");
  checks.Expect(scalar.front() == T{-83} &&
                    std::all_of(scalar.begin() + 1 + n, scalar.end(),
                                [](T value) { return value == T{-83}; }),
                "scalar WORK guards");
  checks.Expect(
      packing.front() == T{-89} &&
          std::all_of(packing.begin() + 1 + packed_count, packing.end(),
                      [](T value) { return value == T{-89}; }),
      "packing guards");
  for (std::size_t i = 0; i < integers.size(); ++i) {
    if (i < 16 || i >= 16 + integer_bytes) {
      checks.Expect(integers[i] == std::byte{0x61}, "native INTEGER guards");
    }
  }
  const auto saved_scalar = scalar;
  const auto saved_packing = packing;
  const auto saved_integers = integers;
  const auto rejected =
      Inverse(provider, triangle == kUpper ? kLower : kUpper, hermitian,
              factors, raw, plan, workspace, report);
  checks.Expect(rejected.code() == asc::ErrorCode::kInvalidState &&
                    !report.called_provider && !report.native_info,
                "stale inverse plan rejected");
  checks.Expect(scalar == saved_scalar && packing == saved_packing &&
                    integers == saved_integers,
                "stale inverse plan preserves scratch");
}

void CheckFactorPivots(Checks& checks, asc::extent_t n,
                       asc::DenseBlasTriangle triangle, bool singular,
                       const std::array<asc::index_t, 7>& pivots) {
  for (asc::extent_t i = 0; i < n; ++i) {
    asc::index_t expected_pivot = static_cast<asc::index_t>(i + 1);
    if (!singular && n != 1 && i != 2) {
      const asc::index_t first = i < 2 ? 1 : 4;
      expected_pivot = -(first + (triangle == kLower ? 1 : 0));
    }
    checks.Expect(pivots[i + 1] == expected_pivot, "signed one-based pivots");
  }
  checks.Expect(pivots.front() == -79 &&
                    std::all_of(pivots.begin() + 1 + n, pivots.end(),
                                [](auto value) { return value == -79; }),
                "public pivot guards");
}

template <typename T>
void Case(Checks& checks, const asc::ReferenceLapackProvider& provider,
          asc::extent_t n, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout layout, bool hermitian, int exponent,
          bool singular) {
  ++checks.workflows;
  std::array<T, 18> data;
  std::array<T, 20> packing;
  std::array<asc::index_t, 7> pivots;
  alignas(std::max_align_t) std::array<std::byte, 96> integers;
  data.fill(T{-71});
  packing.fill(T{-73});
  pivots.fill(-79);
  integers.fill(std::byte{0x59});
  const auto expected =
      Initialize(data, n, triangle, layout, hermitian, exponent, singular);
  const auto matrix = Take(asc::DenseBlasPackedMatrixView<T>::Create(
      data.data() + 1, n, layout, {data.data(), sizeof(data), kHost}));
  const auto pivot_view = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      pivots.data() + 1, n, 1, {pivots.data(), sizeof(pivots), kHost}));
  const auto plan =
      Take(Query(provider, triangle, hermitian, matrix, pivot_view));
  asc::LapackWorkspace workspace;
  const auto packing_entries =
      static_cast<std::size_t>(plan.regions[kPacking].minimum_entries);
  const auto integer_bytes =
      static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
      plan.regions[kInteger].entry_bytes;
  if (n != 0) {
    workspace.regions[kPacking] = {packing.data() + 1,
                                   packing_entries * sizeof(T), kHost};
    workspace.regions[kInteger] = {integers.data() + 16, integer_bytes, kHost};
  }
  asc::LapackReport report;
  const auto status = Factor(provider, triangle, hermitian, matrix, pivot_view,
                             plan, workspace, report);
  CheckReport(checks, n, triangle, singular, status, report);
  checks.Expect(data == expected, "A = D factors and matrix guards");
  CheckFactorPivots(checks, n, triangle, singular, pivots);
  checks.Expect(
      packing.front() == T{-73} &&
          std::all_of(packing.begin() + 1 + packing_entries, packing.end(),
                      [](T value) { return value == T{-73}; }),
      "scalar workspace guards");
  for (std::size_t i = 0; i < integers.size(); ++i) {
    if (i < 16 || i >= 16 + integer_bytes) {
      checks.Expect(integers[i] == std::byte{0x59}, "integer workspace guards");
    }
  }
  InvertWorkflow(checks, provider, triangle, hermitian, singular, matrix,
                 pivots);
  if (singular || n == 0) {
    checks.Expect(data == expected, "singular or empty inverse preserves AP");
  } else {
    CheckInverse(checks, n, triangle, layout, hermitian, expected, data);
  }
  const auto entries = static_cast<std::size_t>(n * (n + 1) / 2);
  checks.Expect(data.front() == T{-71} &&
                    std::all_of(data.begin() + 1 + entries, data.end(),
                                [](T value) { return value == T{-71}; }),
                "inverse AP guards");
  const auto before_packing = packing;
  const auto before_integers = integers;
  const auto before_pivots = pivots;
  const auto before_data = data;
  const auto rejected =
      Factor(provider, triangle == kUpper ? kLower : kUpper, hermitian, matrix,
             pivot_view, plan, workspace, report);
  checks.Expect(rejected.code() == asc::ErrorCode::kInvalidState &&
                    !report.called_provider && !report.native_info,
                "stale triangle rejected before native call");
  checks.Expect(data == before_data && pivots == before_pivots &&
                    packing == before_packing && integers == before_integers,
                "stale plan preserves all operands and scratch");
}

template <typename T>
void Run(Checks& checks, const asc::ReferenceLapackProvider& provider,
         bool hermitian) {
  for (asc::extent_t n : {0, 1, 2, 5}) {
    for (auto triangle : {kUpper, kLower}) {
      for (auto layout : {kColumn, kRow}) {
        for (int exponent : {-20, 0, 20}) {
          for (bool singular : {false, true}) {
            Case<T>(checks, provider, n, triangle, layout, hermitian, exponent,
                    singular);
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  asc_lapack_test::NormalReturnGuard normal_return;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Checks checks;
  Run<float>(checks, provider, false);
  Run<double>(checks, provider, false);
  Run<std::complex<float>>(checks, provider, false);
  Run<std::complex<double>>(checks, provider, false);
  Run<std::complex<float>>(checks, provider, true);
  Run<std::complex<double>>(checks, provider, true);
  checks.Expect(checks.workflows == 576, "all public workflows executed");
  std::printf("packed inverse installed consumer: %d workflows, %d failures\n",
              checks.workflows, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
