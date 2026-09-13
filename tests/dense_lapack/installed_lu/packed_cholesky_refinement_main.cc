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
#include "asc/dense/providers/lapack_cholesky_packed_refinement.h"
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
        std::fprintf(stderr, "Packed refinement failure at check %zu\n",
                     checks);
      }
      ++failures;
    }
  }
};

using Wide = std::complex<long double>;

struct Profile {
  asc::extent_t n;
  asc::extent_t nrhs;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout af_layout;
  asc::DenseBlasLayout b_layout;
  asc::DenseBlasLayout x_layout;
};

template <typename Visit>
void Slots(const Profile& p, asc::DenseBlasLayout layout, const Visit& visit) {
  std::size_t offset = 1;
  for (asc::extent_t major = 0; major < p.n; ++major) {
    for (asc::extent_t minor = 0; minor < p.n; ++minor) {
      const auto i = layout == kColumn ? minor : major;
      const auto j = layout == kColumn ? major : minor;
      if (p.triangle == kUpper ? i <= j : i >= j) {
        visit(offset++, i, j);
      }
    }
  }
}

asc::extent_t Leading(const Profile& p, asc::DenseBlasLayout layout) {
  return (layout == kRow ? p.nrhs : p.n) + 2;
}

std::size_t Offset(const Profile& p, asc::DenseBlasLayout layout,
                   asc::extent_t i, asc::extent_t j) {
  return static_cast<std::size_t>(layout == kRow ? i * Leading(p, layout) + j
                                                 : j * Leading(p, layout) + i) +
         1;
}

template <typename T>
T Narrow(Wide value) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(value.real()), static_cast<Real>(value.imag())};
  } else {
    return static_cast<Real>(value.real());
  }
}

template <typename T>
Wide Widen(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}

long double Abs1(Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}

// Explicit triangular fixture, formed independently of provider algorithms.
// A=L*L^H is positive definite with diagonal 2,...,N+1 and bounded
// off-diagonals.
template <typename T>
Wide Lower(asc::extent_t i, asc::extent_t j) {
  if (i < j) {
    return {};
  }
  if (i == j) {
    return {static_cast<long double>(i + 2), 0};
  }
  const long double imaginary = (i + j) % 2 == 0 ? 0.25L : -0.25L;
  return {0.25L, asc::DenseBlasComplex<T> ? imaginary : 0};
}

template <typename T>
Wide Exact(asc::extent_t i, asc::extent_t j) {
  return {static_cast<long double>((i + 2 * j) % 5 - 2) / 4,
          asc::DenseBlasComplex<T>
              ? static_cast<long double>((i + j) % 3 - 1) / 4
              : 0};
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
  std::array<T, 128> packing;
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
        std::fprintf(stderr, "PPRFS test workspace exceeds actual backing\n");
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

long double Ratio(long double numerator, long double denominator) {
  if (denominator != 0) {
    return numerator / denominator;
  }
  return numerator == 0 ? 0 : std::numeric_limits<long double>::infinity();
}

template <typename T>
struct OperationViews {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasPackedMatrixView<const T> a;
  asc::DenseBlasPackedMatrixView<const T> af;
  asc::DenseBlasMatrixView<const T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> f;
  asc::DenseBlasVectorView<Real> e;
};

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 32> original;
  std::array<T, 32> factors;
  std::array<T, 64> rhs;
  std::array<T, 64> solution;
  std::array<Real, 8> ferr;
  std::array<Real, 8> berr;
  std::array<Wide, 25> matrix{};
  explicit Fixture(const Profile& p, int exponent) {
    original.fill(T{-19});
    factors.fill(T{-23});
    rhs.fill(T{-29});
    solution.fill(T{-31});
    ferr.fill(Real{-37});
    berr.fill(Real{-41});
    const auto scale = std::ldexp(1.0L, exponent);
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.n; ++j) {
        Wide value{};
        for (asc::extent_t k = 0; k < p.n; ++k) {
          value += Lower<T>(i, k) * std::conj(Lower<T>(j, k));
        }
        matrix[static_cast<std::size_t>(i * p.n + j)] =
            Widen(Narrow<T>(value * scale));
      }
    }
    Slots(p, p.a_layout, [&](std::size_t slot, auto i, auto j) {
      original[slot] = Narrow<T>(matrix[static_cast<std::size_t>(i * p.n + j)]);
      if constexpr (asc::DenseBlasComplex<T>) {
        if (i == j) {
          original[slot].imag(std::numeric_limits<Real>::quiet_NaN());
        }
      }
    });
    Slots(p, p.af_layout, [&](std::size_t slot, auto i, auto j) {
      factors[slot] = Narrow<T>(
          (p.triangle == kUpper ? std::conj(Lower<T>(j, i)) : Lower<T>(i, j)) *
          std::sqrt(scale));
    });
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        Wide value{};
        for (asc::extent_t k = 0; k < p.n; ++k) {
          value +=
              matrix[static_cast<std::size_t>(i * p.n + k)] * Exact<T>(k, j);
        }
        rhs[Offset(p, p.b_layout, i, j)] = Narrow<T>(value);
        solution[Offset(p, p.x_layout, i, j)] =
            Narrow<T>(Exact<T>(i, j) * 0.75L);
      }
    }
  }
  OperationViews<T> Views(const Profile& p) {
    const auto a = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        original.data() + 1, p.n, p.a_layout,
        {original.data(), sizeof(original), kHost}));
    const auto af = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        factors.data() + 1, p.n, p.af_layout,
        {factors.data(), sizeof(factors), kHost}));
    const auto b = Take(asc::DenseBlasMatrixView<const T>::Create(
        rhs.data() + 1, p.n, p.nrhs, p.b_layout, Leading(p, p.b_layout),
        {rhs.data(), sizeof(rhs), kHost}));
    const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
        solution.data() + 1, p.n, p.nrhs, p.x_layout, Leading(p, p.x_layout),
        {solution.data(), sizeof(solution), kHost}));
    const auto f = Take(asc::DenseBlasVectorView<Real>::Create(
        ferr.data() + 1, p.nrhs, 1, {ferr.data(), sizeof(ferr), kHost}));
    const auto e = Take(asc::DenseBlasVectorView<Real>::Create(
        berr.data() + 1, p.nrhs, 1, {berr.data(), sizeof(berr), kHost}));
    return {a, af, b, x, f, e};
  }
  [[nodiscard]] bool InputsMatch(const Fixture& before) const {
    return SameBytes(original, before.original) &&
           SameBytes(factors, before.factors) && SameBytes(rhs, before.rhs);
  }
  void Quality(const Profile& p, Checks& checks) const {
    const bool active = p.n != 0 && p.nrhs != 0;
    const long double epsilon = std::numeric_limits<Real>::epsilon();
    for (asc::extent_t j = 0; j < p.nrhs; ++j) {
      const auto slot = static_cast<std::size_t>(j) + 1;
      if (!active) {
        checks.Expect(ferr[slot] == 0 && berr[slot] == 0);
        continue;
      }
      long double matrix_norm = 0;
      long double x_norm = 0;
      long double b_norm = 0;
      long double residual_norm = 0;
      long double x_max = 0;
      long double forward_max = 0;
      for (asc::extent_t i = 0; i < p.n; ++i) {
        const Wide actual = Widen(solution[Offset(p, p.x_layout, i, j)]);
        const Wide original_b = Widen(rhs[Offset(p, p.b_layout, i, j)]);
        long double column = 0;
        Wide residual = -original_b;
        for (asc::extent_t k = 0; k < p.n; ++k) {
          column += Abs1(matrix[static_cast<std::size_t>(k * p.n + i)]);
          residual += matrix[static_cast<std::size_t>(i * p.n + k)] *
                      Widen(solution[Offset(p, p.x_layout, k, j)]);
        }
        matrix_norm = std::max(matrix_norm, column);
        x_norm += Abs1(actual);
        b_norm += Abs1(original_b);
        residual_norm += Abs1(residual);
        x_max = std::max(x_max, Abs1(actual));
        forward_max = std::max(forward_max, Abs1(actual - Exact<T>(i, j)));
      }
      const auto denominator = matrix_norm * x_norm + b_norm;
      const auto residual = Ratio(residual_norm, denominator);
      const auto forward = Ratio(forward_max, x_max);
      // These small explicit factors are well conditioned. Bounds allow
      // dimension-scaled arithmetic error; no additive one hides zero cases.
      checks.Expect(std::isfinite(residual) && residual <= 64 * p.n * epsilon);
      checks.Expect(std::isfinite(forward) && forward <= 128 * p.n * epsilon);
      checks.Expect(std::isfinite(ferr[slot]) && ferr[slot] >= 0 &&
                    ferr[slot] <= 1024 * p.n * p.n * epsilon);
      // For B=X=0 the source adds SAFE1 to both zero numerator and
      // denominator, giving exactly one. This report does not measure the
      // independently normalized zero residual used above.
      checks.Expect(std::isfinite(berr[slot]) && berr[slot] >= 0 &&
                    (denominator == 0 ? berr[slot] == 1
                                      : berr[slot] <= 32 * p.n * epsilon));
      // Fixture-specific estimate check, not a universal FERR certification.
      checks.Expect(forward <= 4 * static_cast<long double>(ferr[slot]));
    }
  }
  void Guards(const Profile& p, Checks& checks) const {
    for (std::size_t slot = 0; slot < solution.size(); ++slot) {
      bool logical = false;
      for (asc::extent_t i = 0; i < p.n; ++i) {
        for (asc::extent_t j = 0; j < p.nrhs; ++j) {
          logical = logical || slot == Offset(p, p.x_layout, i, j);
        }
      }
      if (!logical) {
        checks.Expect(solution[slot] == T{-31});
      }
    }
    Guard(ferr, 1, static_cast<std::size_t>(p.nrhs), Real{-37}, checks);
    Guard(berr, 1, static_cast<std::size_t>(p.nrhs), Real{-41}, checks);
  }
};

template <typename T>
void Workflow(const asc::ReferenceLapackProvider& provider, const Profile& p,
              int exponent, Checks& checks) {
  Fixture<T> fixture(p, exponent);
  const auto before = fixture;
  const auto v = fixture.Views(p);
  const auto plan = Take(asc::QueryPprfsWorkspace(provider, p.triangle, v.a,
                                                  v.af, v.b, v.x, v.f, v.e));
  checks.Expect(fixture.InputsMatch(before) &&
                SameBytes(fixture.solution, before.solution) &&
                SameBytes(fixture.ferr, before.ferr) &&
                SameBytes(fixture.berr, before.berr));
  const bool active = p.n != 0 && p.nrhs != 0;
  const auto packed_count = p.n * (p.n + 1) / 2;
  const auto packing = (p.a_layout == kRow ? packed_count : 0) +
                       (p.af_layout == kRow ? packed_count : 0) +
                       (p.b_layout == kRow ? p.n * p.nrhs : 0) +
                       (p.x_layout == kRow ? p.n * p.nrhs : 0);
  checks.Expect(plan.regions[kPacking].minimum_entries ==
                (active ? packing : 0));
  checks.Expect(plan.regions[kScalar].minimum_entries ==
                (active ? (asc::DenseBlasComplex<T> ? 2 : 3) * p.n : 0));
  checks.Expect(plan.regions[asc::DenseBlasComplex<T> ? kReal : kInteger]
                    .minimum_entries == (active ? p.n : 0));
  Scratch<T> scratch(plan);
  for (int repeat = 0; repeat < 2; ++repeat) {
    ++checks.profiles;
    asc::LapackReport report;
    const auto status = asc::Pprfs(provider, p.triangle, v.a, v.af, v.b, v.x,
                                   v.f, v.e, plan, scratch.workspace, report);
    checks.Expect(status.ok());
    checks.Expect(report.called_provider == active &&
                  (active ? report.native_info == 0 : !report.native_info));
    checks.Expect(report.outcome == asc::LapackOutcome::kSuccess &&
                  report.output_validity ==
                      asc::LapackOutputValidity::kComplete);
    checks.Expect(fixture.InputsMatch(before));
    fixture.Quality(p, checks);
    fixture.Guards(p, checks);
    scratch.Guards(plan, checks);
  }
}

bool LowerPackedOrder(const Profile& p) {
  return (p.triangle == kLower) == (p.a_layout == kColumn);
}

std::array<long double, 3> TriangleTruth(const Profile& p) {
  if (LowerPackedOrder(p)) {
    return {1, 1, 1};
  }
  // Exact integer certificate for A_U * numerator = 3563 * B.
  static_assert(16 * -2245 + 10283 + 25 * 4731 == 3563 * 26);
  static_assert(-2245 + 9 * 10283 + 2 * 4731 == 3563 * 28);
  static_assert(25 * -2245 + 2 * 10283 + 64 * 4731 == 3563 * 75);
  return {-2245.0L / 3563, 10283.0L / 3563, 4731.0L / 3563};
}

template <typename T>
void PrepareTriangle(Fixture<T>& fixture, const Profile& p) {
  // Identical packed coefficients describe two different positive-definite
  // matrices when UPLO changes. Supply the matching factor for the selected
  // matrix, preserving the provenance precondition while testing sensitivity.
  const std::array<long double, 6> packed{16, 1, 9, 25, 2, 64};
  for (std::size_t i = 0; i < packed.size(); ++i) {
    fixture.original[i + 1] = Narrow<T>(packed[i]);
  }
  const long double middle = LowerPackedOrder(p) ? 25 : 9;
  const long double corner = LowerPackedOrder(p) ? 9 : 25;
  const std::array<long double, 9> a{16, 1,      corner, 1, middle,
                                     2,  corner, 2,      64};
  for (std::size_t i = 0; i < a.size(); ++i) {
    fixture.matrix[i] = a[i];
  }
  const auto diagonal = std::sqrt(middle - 0.0625L);
  const auto last = (2 - corner / 16) / diagonal;
  const std::array<long double, 9> lower{
      4,          0,        0,
      0.25L,      diagonal, 0,
      corner / 4, last,     std::sqrt(64 - corner * corner / 16 - last * last)};
  Slots(p, p.af_layout,
        [&](std::size_t slot, asc::extent_t i, asc::extent_t j) {
          const auto index = p.triangle == kLower ? 3 * i + j : 3 * j + i;
          fixture.factors[slot] =
              Narrow<T>(lower[static_cast<std::size_t>(index)]);
        });
  const std::array<long double, 3> b{26, 28, 75};
  const auto truth = TriangleTruth(p);
  for (asc::extent_t i = 0; i < 3; ++i) {
    const auto at = static_cast<std::size_t>(i);
    fixture.rhs[Offset(p, p.b_layout, i, 0)] = Narrow<T>(b[at]);
    fixture.solution[Offset(p, p.x_layout, i, 0)] =
        Narrow<T>(0.75L * truth[at]);
  }
}

template <typename T>
void TriangleQuality(const Fixture<T>& fixture, const Profile& p,
                     Checks& checks) {
  const auto truth = TriangleTruth(p);
  long double matrix_norm = 0;
  long double x_norm = 0;
  long double b_norm = 0;
  long double residual_norm = 0;
  long double forward_norm = 0;
  long double truth_norm = 0;
  for (asc::extent_t i = 0; i < 3; ++i) {
    const auto value = Widen(fixture.solution[Offset(p, p.x_layout, i, 0)]);
    x_norm += Abs1(value);
    forward_norm += Abs1(value - truth[static_cast<std::size_t>(i)]);
    truth_norm += std::abs(truth[static_cast<std::size_t>(i)]);
    const auto b = Widen(fixture.rhs[Offset(p, p.b_layout, i, 0)]);
    b_norm += Abs1(b);
    Wide product{};
    long double column = 0;
    for (asc::extent_t j = 0; j < 3; ++j) {
      product += fixture.matrix[static_cast<std::size_t>(3 * i + j)] *
                 Widen(fixture.solution[Offset(p, p.x_layout, j, 0)]);
      column += Abs1(fixture.matrix[static_cast<std::size_t>(3 * j + i)]);
    }
    residual_norm += Abs1(product - b);
    matrix_norm = std::max(matrix_norm, column);
  }
  const auto epsilon =
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
  // The two fixed nonsingular systems use the same dimension-scaled residual,
  // forward and fixture-specific estimate bounds as the original workflows.
  const auto residual = Ratio(residual_norm, matrix_norm * x_norm + b_norm);
  const auto forward = Ratio(forward_norm, truth_norm);
  checks.Expect(std::isfinite(residual) && residual <= 64 * 3 * epsilon);
  checks.Expect(std::isfinite(forward) && forward <= 128 * 3 * epsilon);
  checks.Expect(std::isfinite(fixture.ferr[1]) && fixture.ferr[1] >= 0 &&
                fixture.ferr[1] <= 1024 * 9 * epsilon);
  checks.Expect(std::isfinite(fixture.berr[1]) && fixture.berr[1] >= 0 &&
                fixture.berr[1] <= 32 * 3 * epsilon);
  checks.Expect(forward <= 4 * fixture.ferr[1]);
  checks.Expect(
      LowerPackedOrder(p) ||
      Abs1(Widen(fixture.solution[Offset(p, p.x_layout, 0, 0)]) - Wide{1}) > 1);
}

template <typename T>
void TriangleWorkflow(const asc::ReferenceLapackProvider& provider,
                      const Profile& p, Checks& checks) {
  Fixture<T> fixture(p, 0);
  PrepareTriangle(fixture, p);
  const auto before = fixture;
  const auto v = fixture.Views(p);
  const auto plan = Take(asc::QueryPprfsWorkspace(provider, p.triangle, v.a,
                                                  v.af, v.b, v.x, v.f, v.e));
  Scratch<T> scratch(plan);
  asc::LapackReport report;
  const auto status = asc::Pprfs(provider, p.triangle, v.a, v.af, v.b, v.x, v.f,
                                 v.e, plan, scratch.workspace, report);
  checks.Expect(installed_internal::Succeeded(status, report));
  checks.Expect(fixture.InputsMatch(before));
  TriangleQuality(fixture, p, checks);
  fixture.Guards(p, checks);
  scratch.Guards(plan, checks);
  ++checks.profiles;
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
    for (const auto al : {kRow, kColumn}) {
      for (const auto afl : {kRow, kColumn}) {
        for (const auto bl : {kRow, kColumn}) {
          for (const auto xl : {kRow, kColumn}) {
            TriangleWorkflow<T>(provider, {3, 1, triangle, al, afl, bl, xl},
                                checks);
            for (const auto n : {0, 1, 2, 3, 5}) {
              for (const auto nrhs : {0, 1, 3}) {
                for (const auto exponent : exponents) {
                  Workflow<T>(provider, {n, nrhs, triangle, al, afl, bl, xl},
                              exponent, checks);
                }
              }
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Checks checks;
  Run<float>(provider, checks);
  Run<double>(provider, checks);
  Run<std::complex<float>>(provider, checks);
  Run<std::complex<double>>(provider, checks);
  std::printf(
      "Packed Cholesky refinement: %zu profiles, %zu checks, %zu failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
