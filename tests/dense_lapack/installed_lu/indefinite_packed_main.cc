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
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed.h"
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
        std::fprintf(stderr, "Packed factor consumer: %s (workflow %d)\n",
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
  std::printf("packed installed consumer: %d workflows, %d failures\n",
              checks.workflows, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
