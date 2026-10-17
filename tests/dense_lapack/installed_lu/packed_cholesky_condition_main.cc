#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_condition.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kScalar =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScalar);
constexpr auto kReal =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

struct Checks {
  std::size_t profiles = 0;
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      if (failures < 20) {
        std::fprintf(stderr, "Packed condition failure at check %zu\n", checks);
      }
      ++failures;
    }
  }
};

struct Profile {
  asc::extent_t n;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
};

template <typename Visit>
void Slots(const Profile& p, const Visit& visit) {
  std::size_t offset = 1;
  for (asc::extent_t major = 0; major < p.n; ++major) {
    for (asc::extent_t minor = 0; minor < p.n; ++minor) {
      const auto i = p.layout == kColumn ? minor : major;
      const auto j = p.layout == kColumn ? major : minor;
      if (p.triangle == kUpper ? i <= j : i >= j) {
        visit(offset++, i, j);
      }
    }
  }
}

template <typename T, std::size_t N>
bool SameBytes(const std::array<T, N>& a, const std::array<T, N>& b) {
  const auto left = std::as_bytes(std::span(a));
  const auto right = std::as_bytes(std::span(b));
  return std::equal(left.begin(), left.end(), right.begin(), right.end());
}

template <typename T, std::size_t N>
void Guard(const std::array<T, N>& data, std::size_t offset, std::size_t count,
           T sentinel, Checks& checks) {
  for (std::size_t i = 0; i < data.size(); ++i) {
    if (i < offset || i >= offset + count) {
      checks.Expect(data[i] == sentinel);
    }
  }
}

template <typename T>
struct Scratch {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 64> scalar;
  std::array<T, 32> packing;
  std::array<Real, 8> real;
  alignas(std::max_align_t) std::array<std::byte, 64> integer;
  asc::LapackWorkspace workspace;
  explicit Scratch(const asc::LapackWorkspacePlan& plan) {
    scalar.fill(T{-43});
    packing.fill(T{-47});
    real.fill(Real{-53});
    integer.fill(std::byte{0x5a});
    const std::array<void*, 4> pointers{scalar.data() + 1, packing.data() + 1,
                                        real.data() + 1, integer.data() + 8};
    const std::array kinds{kScalar, kPacking, kReal, kInteger};
    const std::array capacities{
        sizeof(T) * (scalar.size() - 2), sizeof(T) * (packing.size() - 2),
        sizeof(Real) * (real.size() - 2), integer.size() - 16};
    for (std::size_t i = 0; i < kinds.size(); ++i) {
      const auto& region = plan.regions[kinds[i]];
      const auto bytes = static_cast<std::size_t>(region.preferred_entries) *
                         region.entry_bytes;
      if (bytes > capacities[i]) {
        std::fprintf(stderr, "PPCON test workspace exceeds actual backing\n");
        std::abort();
      }
      if (bytes != 0) {
        workspace.regions[kinds[i]] = {pointers[i], bytes, kHost};
      }
    }
  }
  void Guards(const asc::LapackWorkspacePlan& plan, Checks& checks) const {
    Guard(scalar, 1, plan.regions[kScalar].preferred_entries, T{-43}, checks);
    Guard(packing, 1, plan.regions[kPacking].preferred_entries, T{-47}, checks);
    Guard(real, 1, plan.regions[kReal].preferred_entries, Real{-53}, checks);
    const auto& region = plan.regions[kInteger];
    Guard(
        integer, 8,
        static_cast<std::size_t>(region.preferred_entries) * region.entry_bytes,
        std::byte{0x5a}, checks);
  }
};

template <typename Real>
void Relative(Real actual, long double expected, Checks& checks) {
  // Diagonal inverse norms are exact maxima. For the two-by-two fixtures,
  // LACN2 reaches the maximizing first basis column; at most two solves and
  // the final ratio contribute rounding. Bound 64 scalar rounding units.
  const auto error = std::abs(static_cast<long double>(actual) - expected);
  checks.Expect(std::isfinite(actual));
  checks.Expect(expected == 0 ? actual == 0
                              : error / std::abs(expected) <=
                                    64 * std::numeric_limits<Real>::epsilon());
}

template <typename T>
void Workflow(const asc::ReferenceLapackProvider& provider, const Profile& p,
              int exponent, bool coupled, bool zero_norm, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 32> matrix;
  matrix.fill(T{-19});
  const Real scale = std::ldexp(Real{1}, exponent);
  Slots(p, [&](std::size_t offset, auto i, auto j) {
    if (zero_norm || (!coupled && i != j)) {
      matrix[offset] = T{};
    } else if (i == j) {
      matrix[offset] = static_cast<Real>(i + (coupled ? 2 : 1)) * scale;
    } else {
      if constexpr (asc::DenseBlasComplex<T>) {
        matrix[offset] = {0, p.triangle == kUpper ? -scale : scale};
      } else {
        matrix[offset] = scale;
      }
    }
  });
  const auto old = matrix;
  const auto a = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
      matrix.data() + 1, p.n, p.layout,
      {matrix.data(), sizeof(matrix), kHost}));
  // L=[[2,0],[1,3]] or [[2,0],[i,3]] gives ||A||1=12 and
  // ||inv(A)||1=1/3, hence exact reciprocal condition 1/4. Diagonal
  // factors (1,...,N) give original norm N^2 and inverse norm one.
  const Real norm =
      zero_norm ? 0
                : std::ldexp(static_cast<Real>(coupled ? 12 : p.n * p.n),
                             2 * exponent);
  std::array<Real, 3> condition{Real{-29}, Real{-29}, Real{-29}};
  const auto plan = Take(
      asc::QueryPpconWorkspace(provider, p.triangle, a, norm, condition[1]));
  checks.Expect(SameBytes(matrix, old) && condition[1] == Real{-29});
  const bool active = p.n != 0 && !zero_norm;
  checks.Expect(plan.regions[kScalar].minimum_entries ==
                (active ? (asc::DenseBlasComplex<T> ? 2 : 3) * p.n : 0));
  checks.Expect(plan.regions[kPacking].minimum_entries ==
                (active && p.layout == kRow ? p.n * (p.n + 1) / 2 : 0));
  Scratch<T> scratch(plan);
  for (int repeat = 0; repeat < 2; ++repeat) {
    ++checks.profiles;
    asc::LapackReport report;
    const auto status = asc::Ppcon(provider, p.triangle, a, norm, condition[1],
                                   plan, scratch.workspace, report);
    checks.Expect(status.ok());
    checks.Expect(report.called_provider == active &&
                  (active ? report.native_info == 0 : !report.native_info));
    checks.Expect(report.outcome == asc::LapackOutcome::kSuccess &&
                  report.output_validity ==
                      asc::LapackOutputValidity::kComplete);
    long double expected = 1;
    if (p.n != 0) {
      expected = zero_norm ? 0 : 1.0L / (coupled ? 4 : p.n * p.n);
    }
    Relative(condition[1], expected, checks);
    checks.Expect(SameBytes(matrix, old));
    checks.Expect(condition.front() == Real{-29} &&
                  condition.back() == Real{-29});
    scratch.Guards(plan, checks);
  }
}

template <typename T>
void Run(const asc::ReferenceLapackProvider& provider, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array exponents{
      -20,
      -1,
      0,
      10,
      20,
      (std::numeric_limits<Real>::min_exponent - 1) / 2,
      (std::numeric_limits<Real>::max_exponent - 12) / 2};
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kRow, kColumn}) {
      for (const auto n : {0, 1, 2, 3, 5}) {
        for (const auto exponent : exponents) {
          Workflow<T>(provider, {n, triangle, layout}, exponent, false, false,
                      checks);
          if (n == 2) {
            Workflow<T>(provider, {n, triangle, layout}, exponent, true, false,
                        checks);
          }
        }
        Workflow<T>(provider, {n, triangle, layout}, 0, false, true, checks);
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  Checks checks;
  Run<float>(provider, checks);
  Run<double>(provider, checks);
  Run<std::complex<float>>(provider, checks);
  Run<std::complex<double>>(provider, checks);
  std::printf(
      "Packed Cholesky condition: %zu profiles, %zu checks, %zu failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
