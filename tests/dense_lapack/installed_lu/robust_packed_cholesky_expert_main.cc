#include <algorithm>
#include <array>
#include <barrier>
#include <cfenv>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <span>
#include <string_view>
#include <thread>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_cholesky_packed_robust.h"
#include "factorization_support.h"
#include "normal_return_guard.h"
#if defined(ASC_ROBUST_PPSVX_LOAD_OBSERVATION)
#include "observation.h"
#endif
#if defined(ASC_ROBUST_PPSVX_ALLOCATION)
#include "allocation_audit.h"
#include "allocation_probe.h"
#endif

namespace {
// The independent extreme oracle needs exponent headroom for inverse products.
// This is a test-platform requirement, not a claim that long double is always
// wider. The admitted GNU11 x86_64 profile satisfies it. Other oracle profiles
// must be reviewed before they can receive extreme numerical verification.
static_assert(std::numeric_limits<long double>::max_exponent >= 4096 &&
                  std::numeric_limits<long double>::min_exponent <= -4096,
              "Robust PPSVX extreme oracle requires a wider exponent range");
using installed_internal::Take;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kErrors =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch);
enum class Mode : std::uint8_t { kNew, kEquilibrate, kSupplied, kScaled };

struct Checks {
  std::size_t profiles = 0;
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value,
              std::source_location location = std::source_location::current()) {
    ++checks;
    if (!value) {
      if (failures < 20) {
        std::fprintf(stderr, "Packed expert failure at check %zu line %u\n",
                     checks, location.line());
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
  Mode mode;
  bool diagonal = false;
  bool extreme = false;
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
Wide Lower(asc::extent_t i, asc::extent_t j, bool diagonal = false) {
  if (i < j) {
    return {};
  }
  if (i == j) {
    return {static_cast<long double>(i + 2), 0};
  }
  if (diagonal) {
    return {};
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
  alignas(std::max_align_t) std::array<std::byte, 65536> storage;
  asc::LapackWorkspace workspace;
  explicit Scratch(const asc::LapackWorkspacePlan& plan) {
    storage.fill(std::byte{0x5a});
    const auto& region = plan.regions[kErrors];
    const auto bytes =
        static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes;
    if (bytes > storage.size() - 64 || region.alignment > 32) {
      std::abort();
    }
    if (bytes != 0) {
      workspace.regions[kErrors] = {storage.data() + 32, bytes, kHost};
    }
  }
  void Guards(const asc::LapackWorkspacePlan& plan, Checks& checks) const {
    const auto& region = plan.regions[kErrors];
    const auto bytes =
        static_cast<std::size_t>(region.minimum_entries) * region.entry_bytes;
    for (std::size_t i = 0; i < 32; ++i) {
      checks.Expect(storage[i] == std::byte{0x5a});
      checks.Expect(storage[32 + bytes + i] == std::byte{0x5a});
    }
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
  asc::DenseBlasPackedMatrixView<T> a;
  asc::DenseBlasPackedMatrixView<T> af;
  asc::DenseBlasMatrixView<T> b;
  asc::DenseBlasMatrixView<T> x;
  asc::DenseBlasVectorView<Real> s;
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
  std::array<Real, 8> scales;
  std::array<Real, 8> ferr;
  std::array<Real, 8> berr;
  std::array<Wide, 25> matrix{};
  Real rcond = -61;
  asc::LapackCholeskyEquilibration equilibration =
      asc::LapackCholeskyEquilibration::kDiagonal;

  explicit Fixture(const Profile& p, int exponent) {
    original.fill(T{-19});
    factors.fill(T{-23});
    rhs.fill(T{-29});
    solution.fill(T{-31});
    scales.fill(Real{-59});
    ferr.fill(Real{-37});
    berr.fill(Real{-41});
    const auto scale = std::ldexp(1.0L, exponent);
    for (asc::extent_t i = 0; i < p.n; ++i) {
      if (p.mode == Mode::kScaled) {
        scales[static_cast<std::size_t>(i) + 1] =
            static_cast<Real>(std::ldexp(1.0L, static_cast<int>(i) - 2));
      }
      for (asc::extent_t j = 0; j < p.n; ++j) {
        Wide value{};
        for (asc::extent_t k = 0; k < p.n; ++k) {
          value += Lower<T>(i, k, p.diagonal) *
                   std::conj(Lower<T>(j, k, p.diagonal));
        }
        matrix[static_cast<std::size_t>(i * p.n + j)] =
            Widen(Narrow<T>(value * scale));
      }
    }
    InitializePacked(p, scale);
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        Wide value{};
        for (asc::extent_t k = 0; k < p.n; ++k) {
          value +=
              matrix[static_cast<std::size_t>(i * p.n + k)] * Exact<T>(k, j);
        }
        rhs[Offset(p, p.b_layout, i, j)] = Narrow<T>(value);
      }
    }
  }

  void InitializePacked(const Profile& p, long double scale) {
    Slots(p, p.a_layout, [&](std::size_t slot, auto i, auto j) {
      Wide value = matrix[static_cast<std::size_t>(i * p.n + j)];
      if (p.mode == Mode::kScaled) {
        value *=
            static_cast<long double>(scales[static_cast<std::size_t>(i) + 1]) *
            scales[static_cast<std::size_t>(j) + 1];
      }
      original[slot] = Narrow<T>(value);
      if constexpr (asc::DenseBlasComplex<T>) {
        if (i == j) {
          original[slot].imag(std::numeric_limits<Real>::quiet_NaN());
        }
      }
    });
    if (p.mode != Mode::kSupplied && p.mode != Mode::kScaled) {
      return;
    }
    Slots(p, p.af_layout, [&](std::size_t slot, auto i, auto j) {
      Wide value = (p.triangle == kUpper ? std::conj(Lower<T>(j, i, p.diagonal))
                                         : Lower<T>(i, j, p.diagonal)) *
                   std::sqrt(scale);
      if (p.mode == Mode::kScaled) {
        value *=
            scales[static_cast<std::size_t>(p.triangle == kUpper ? j : i) + 1];
      }
      factors[slot] = Narrow<T>(value);
    });
  }

  OperationViews<T> Views(const Profile& p) {
    const auto a = Take(asc::DenseBlasPackedMatrixView<T>::Create(
        original.data() + 1, p.n, p.a_layout,
        {original.data(), sizeof(original), kHost}));
    const auto af = Take(asc::DenseBlasPackedMatrixView<T>::Create(
        factors.data() + 1, p.n, p.af_layout,
        {factors.data(), sizeof(factors), kHost}));
    const auto b = Take(asc::DenseBlasMatrixView<T>::Create(
        rhs.data() + 1, p.n, p.nrhs, p.b_layout, Leading(p, p.b_layout),
        {rhs.data(), sizeof(rhs), kHost}));
    const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
        solution.data() + 1, p.n, p.nrhs, p.x_layout, Leading(p, p.x_layout),
        {solution.data(), sizeof(solution), kHost}));
    const auto s = Take(asc::DenseBlasVectorView<Real>::Create(
        scales.data() + 1, p.n, 1, {scales.data(), sizeof(scales), kHost}));
    const auto f = Take(asc::DenseBlasVectorView<Real>::Create(
        ferr.data() + 1, p.nrhs, 1, {ferr.data(), sizeof(ferr), kHost}));
    const auto e = Take(asc::DenseBlasVectorView<Real>::Create(
        berr.data() + 1, p.nrhs, 1, {berr.data(), sizeof(berr), kHost}));
    return {a, af, b, x, s, f, e};
  }

  [[nodiscard]] bool Same(const Fixture& before) const {
    return SameBytes(original, before.original) &&
           SameBytes(factors, before.factors) && SameBytes(rhs, before.rhs) &&
           SameBytes(solution, before.solution) &&
           SameBytes(scales, before.scales) && SameBytes(ferr, before.ferr) &&
           SameBytes(berr, before.berr) && rcond == before.rcond &&
           equilibration == before.equilibration;
  }
  // A componentwise denominator may vanish even when the whole RHS does
  // not. Derive that case from the exact fixture equation, independently of
  // the reported bound. Positive diagonal equilibration preserves its zeros.
  [[nodiscard]] bool HasZeroBackwardDenominator(const Profile& p,
                                                const Fixture& before,
                                                asc::extent_t j) const {
    for (asc::extent_t i = 0; i < p.n; ++i) {
      long double denominator =
          Abs1(Widen(before.rhs[Offset(p, p.b_layout, i, j)]));
      for (asc::extent_t k = 0; k < p.n; ++k) {
        denominator += Abs1(matrix[static_cast<std::size_t>(i * p.n + k)]) *
                       Abs1(Exact<T>(k, j));
      }
      if (denominator == 0) {
        return true;
      }
    }
    return false;
  }
  void ExtremeQuality(const Profile& p, const Fixture& before,
                      Checks& checks) const;
  void Quality(const Profile& p, const Fixture& before, Checks& checks) const {
    if (p.extreme) {
      ExtremeQuality(p, before, checks);
      return;
    }
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
        const Wide original_b = Widen(before.rhs[Offset(p, p.b_layout, i, j)]);
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
      // A zero component adds SAFE1 to its zero numerator and denominator,
      // giving exactly one. Other components have small backward errors in
      // these fixtures. This differs from the normwise residual above.
      checks.Expect(std::isfinite(berr[slot]) && berr[slot] >= 0 &&
                    (HasZeroBackwardDenominator(p, before, j)
                         ? berr[slot] == 1
                         : berr[slot] <= 32 * p.n * epsilon));
      // Fixture-specific estimate check, not a universal FERR certification.
      checks.Expect(forward <= 4 * static_cast<long double>(ferr[slot]));
    }
  }
};

template <typename T>
asc::Result<asc::LapackWorkspacePlan> Query(
    const asc::ReferenceLapackProvider& provider, const Profile& p,
    const OperationViews<T>& v, const Fixture<T>& fixture) {
  if (p.mode == Mode::kNew) {
    return asc::QueryRobustPpsvxWorkspace(provider, p.triangle, v.a, v.af, v.b,
                                          v.x, v.f, v.e, fixture.rcond);
  }
  if (p.mode == Mode::kEquilibrate) {
    return asc::QueryRobustPpsvxEquilibratedWorkspace(
        provider, p.triangle, v.a, v.af, fixture.equilibration, v.s, v.b, v.x,
        v.f, v.e, fixture.rcond);
  }
  const auto equed = p.mode == Mode::kScaled
                         ? asc::LapackCholeskyEquilibration::kDiagonal
                         : asc::LapackCholeskyEquilibration::kNone;
  return asc::QueryRobustPpsvxFactoredWorkspace(provider, p.triangle, v.a, v.af,
                                                equed, v.s, v.b, v.x, v.f, v.e,
                                                fixture.rcond);
}

template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                    const Profile& p, const OperationViews<T>& v, Fixture<T>& f,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& work,
                    asc::LapackReport& report) {
  if (p.mode == Mode::kNew) {
    return asc::RobustPpsvx(provider, p.triangle, v.a, v.af, v.b, v.x, v.f, v.e,
                            f.rcond, plan, work, report);
  }
  if (p.mode == Mode::kEquilibrate) {
    return asc::RobustPpsvxEquilibrated(provider, p.triangle, v.a, v.af,
                                        f.equilibration, v.s, v.b, v.x, v.f,
                                        v.e, f.rcond, plan, work, report);
  }
  const auto equed = p.mode == Mode::kScaled
                         ? asc::LapackCholeskyEquilibration::kDiagonal
                         : asc::LapackCholeskyEquilibration::kNone;
  return asc::RobustPpsvxFactored(provider, p.triangle, v.a, v.af, equed, v.s,
                                  v.b, v.x, v.f, v.e, f.rcond, plan, work,
                                  report);
}

template <typename T>
bool Scaled(const Profile& p, const Fixture<T>& f) {
  return p.mode == Mode::kScaled ||
         (p.mode == Mode::kEquilibrate &&
          f.equilibration == asc::LapackCholeskyEquilibration::kDiagonal);
}

template <typename T>
long double Scaling(const Profile& p, const Fixture<T>& f, asc::extent_t i) {
  return Scaled(p, f) ? f.scales[static_cast<std::size_t>(i) + 1] : 1;
}

// Independent small-matrix oracle: pivoted Gauss-Jordan in long double.
// It is test-only and uses neither the robust operator nor provider factors.
template <typename T>
std::array<Wide, 25> Inverse(const Profile& p, const Fixture<T>& f) {
  std::array<Wide, 25> a{};
  std::array<Wide, 25> inverse{};
  for (asc::extent_t i = 0; i < p.n; ++i) {
    inverse[static_cast<std::size_t>(i * p.n + i)] = 1;
    for (asc::extent_t j = 0; j < p.n; ++j) {
      a[static_cast<std::size_t>(i * p.n + j)] =
          f.matrix[static_cast<std::size_t>(i * p.n + j)] * Scaling(p, f, i) *
          Scaling(p, f, j);
    }
  }
  for (asc::extent_t j = 0; j < p.n; ++j) {
    auto pivot = j;
    for (asc::extent_t i = j + 1; i < p.n; ++i) {
      if (std::abs(a[static_cast<std::size_t>(i * p.n + j)]) >
          std::abs(a[static_cast<std::size_t>(pivot * p.n + j)])) {
        pivot = i;
      }
    }
    for (asc::extent_t k = 0; k < p.n; ++k) {
      std::swap(a[static_cast<std::size_t>(pivot * p.n + k)],
                a[static_cast<std::size_t>(j * p.n + k)]);
      std::swap(inverse[static_cast<std::size_t>(pivot * p.n + k)],
                inverse[static_cast<std::size_t>(j * p.n + k)]);
    }
    const auto diagonal = a[static_cast<std::size_t>(j * p.n + j)];
    for (asc::extent_t k = 0; k < p.n; ++k) {
      a[static_cast<std::size_t>(j * p.n + k)] /= diagonal;
      inverse[static_cast<std::size_t>(j * p.n + k)] /= diagonal;
    }
    for (asc::extent_t i = 0; i < p.n; ++i) {
      if (i != j) {
        const auto multiplier = a[static_cast<std::size_t>(i * p.n + j)];
        for (asc::extent_t k = 0; k < p.n; ++k) {
          a[static_cast<std::size_t>(i * p.n + k)] -=
              multiplier * a[static_cast<std::size_t>(j * p.n + k)];
          inverse[static_cast<std::size_t>(i * p.n + k)] -=
              multiplier * inverse[static_cast<std::size_t>(j * p.n + k)];
        }
      }
    }
  }
  return inverse;
}

template <typename T>
void Fixture<T>::ExtremeQuality(const Profile& p, const Fixture& before,
                                Checks& checks) const {
  const auto inverse = Inverse(p, *this);
  const long double eps = std::numeric_limits<Real>::epsilon();
  long double norm_a = 0;
  long double norm_inverse = 0;
  long double norm_scaled_inverse = 0;
  for (asc::extent_t i = 0; i < p.n; ++i) {
    long double a_sum = 0;
    long double inverse_sum = 0;
    long double scaled_sum = 0;
    for (asc::extent_t j = 0; j < p.n; ++j) {
      a_sum += std::abs(matrix[static_cast<std::size_t>(i * p.n + j)]) *
               Scaling(p, *this, i) * Scaling(p, *this, j);
      inverse_sum += std::abs(inverse[static_cast<std::size_t>(i * p.n + j)]);
      scaled_sum += Abs1(inverse[static_cast<std::size_t>(i * p.n + j)]) *
                    Scaling(p, *this, i);
    }
    norm_a = std::max(norm_a, a_sum);
    norm_inverse = std::max(norm_inverse, inverse_sum);
    norm_scaled_inverse = std::max(norm_scaled_inverse, scaled_sum);
  }
  const auto expected_condition = p.n == 0 ? 1 : 1 / (norm_a * norm_inverse);
  checks.Expect(
      std::isfinite(rcond) &&
      Ratio(std::abs(rcond - expected_condition), expected_condition) <=
          64 * std::max<asc::extent_t>(1, p.n) * eps);
  for (asc::extent_t j = 0; j < p.nrhs; ++j) {
    long double norm_x = 0;
    long double error = 0;
    for (asc::extent_t i = 0; i < p.n; ++i) {
      const auto actual = Widen(solution[Offset(p, p.x_layout, i, j)]);
      norm_x = std::max(norm_x, Abs1(actual));
      error = std::max(error, Abs1(actual - Exact<T>(i, j)));
      auto residual = Widen(before.rhs[Offset(p, p.b_layout, i, j)]);
      auto denominator = Abs1(residual);
      for (asc::extent_t k = 0; k < p.n; ++k) {
        const auto coefficient = matrix[static_cast<std::size_t>(i * p.n + k)];
        const auto x = Widen(solution[Offset(p, p.x_layout, k, j)]);
        residual -= coefficient * x;
        denominator += Abs1(coefficient) * Abs1(x);
      }
      checks.Expect(Ratio(Abs1(residual), denominator) <= 64 * p.n * eps);
    }
    const auto slot = static_cast<std::size_t>(j) + 1;
    const auto absolute_or_relative = norm_x == 0 ? 1 : norm_x;
    const auto floor_bound = 4 * (p.n + 1) * std::numeric_limits<Real>::min() *
                             norm_scaled_inverse / absolute_or_relative;
    checks.Expect(Ratio(error, norm_x) <= 128 * p.n * eps);
    checks.Expect(std::isfinite(ferr[slot]) && ferr[slot] >= 0 &&
                  ferr[slot] <= 1024 * p.n * p.n * eps + floor_bound);
    checks.Expect(std::isfinite(berr[slot]) && berr[slot] >= 0 &&
                  berr[slot] <= 1);
    checks.Expect(error / absolute_or_relative <= 4 * ferr[slot]);
  }
}

template <typename T>
void Mutation(const Profile& p, const Fixture<T>& f, const Fixture<T>& before,
              Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const long double eps = std::numeric_limits<Real>::epsilon();
  if (p.mode != Mode::kEquilibrate || !Scaled(p, f)) {
    checks.Expect(SameBytes(f.original, before.original));
  } else {
    Slots(p, p.a_layout, [&](std::size_t slot, auto i, auto j) {
      const auto expected = f.matrix[static_cast<std::size_t>(i * p.n + j)] *
                            Scaling(p, f, i) * Scaling(p, f, j);
      checks.Expect(Ratio(Abs1(Widen(f.original[slot]) - expected),
                          Abs1(expected)) <= 8 * eps);
    });
  }
  if (p.mode == Mode::kSupplied || p.mode == Mode::kScaled) {
    checks.Expect(SameBytes(f.factors, before.factors));
  }
  if (p.mode != Mode::kEquilibrate) {
    checks.Expect(SameBytes(f.scales, before.scales));
  } else {
    long double smallest = std::numeric_limits<long double>::infinity();
    long double largest = 0;
    for (asc::extent_t i = 0; i < p.n; ++i) {
      const auto diag = f.matrix[static_cast<std::size_t>(i * p.n + i)].real();
      const auto expected = 1 / std::sqrt(diag);
      checks.Expect(
          std::isfinite(f.scales[static_cast<std::size_t>(i) + 1]) &&
          Ratio(std::abs(f.scales[static_cast<std::size_t>(i) + 1] - expected),
                expected) <= 4 * eps);
      smallest = std::min(smallest, diag);
      largest = std::max(largest, diag);
    }
    const auto small =
        static_cast<long double>(std::numeric_limits<Real>::min()) / eps;
    const bool expected =
        p.n > 0 && (std::sqrt(smallest) / std::sqrt(largest) < 0.1L ||
                    largest < small || largest > 1 / small);
    checks.Expect(Scaled(p, f) == expected);
  }
  if (!Scaled(p, f)) {
    checks.Expect(SameBytes(f.rhs, before.rhs));
  } else {
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        const auto slot = Offset(p, p.b_layout, i, j);
        const auto expected = Widen(before.rhs[slot]) * Scaling(p, f, i);
        checks.Expect(Ratio(Abs1(Widen(f.rhs[slot]) - expected),
                            Abs1(expected)) <= 4 * eps);
      }
    }
  }
}

template <typename T>
void DiagonalCondition(const Profile& p, const Fixture<T>& f, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  long double smallest = std::numeric_limits<long double>::infinity();
  long double largest = 0;
  for (asc::extent_t i = 0; i < p.n; ++i) {
    const auto value = f.matrix[static_cast<std::size_t>(i * p.n + i)].real() *
                       Scaling(p, f, i) * Scaling(p, f, i);
    smallest = std::min(smallest, value);
    largest = std::max(largest, value);
  }
  // For a positive diagonal matrix, reciprocal condition is
  // min(diag)/max(diag). The bound accounts for scaling, square roots and the
  // two triangular solves.
  const auto expected = smallest / largest;
  checks.Expect(std::isfinite(f.rcond) &&
                std::abs(static_cast<long double>(f.rcond) - expected) /
                        expected <=
                    64 * p.n * std::numeric_limits<Real>::epsilon());
}

template <typename T>
void Reconstruction(const Profile& p, const Fixture<T>& f, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Wide, 25> factor{};
  Slots(p, p.af_layout, [&](std::size_t slot, auto i, auto j) {
    factor[static_cast<std::size_t>(i * p.n + j)] = Widen(f.factors[slot]);
  });
  long double error = 0;
  long double norm = 0;
  for (asc::extent_t j = 0; j < p.n; ++j) {
    long double column = 0;
    long double delta = 0;
    for (asc::extent_t i = 0; i < p.n; ++i) {
      Wide actual{};
      for (asc::extent_t k = 0; k < p.n; ++k) {
        if (p.triangle == kLower) {
          actual += factor[static_cast<std::size_t>(i * p.n + k)] *
                    std::conj(factor[static_cast<std::size_t>(j * p.n + k)]);
        } else {
          actual += std::conj(factor[static_cast<std::size_t>(k * p.n + i)]) *
                    factor[static_cast<std::size_t>(k * p.n + j)];
        }
      }
      const auto expected = f.matrix[static_cast<std::size_t>(i * p.n + j)] *
                            Scaling(p, f, i) * Scaling(p, f, j);
      column += Abs1(expected);
      delta += Abs1(actual - expected);
    }
    norm = std::max(norm, column);
    error = std::max(error, delta);
  }
  // Three small matrix products/roundings fit this dimension-scaled bound.
  checks.Expect(Ratio(error, norm) <=
                64 * p.n * std::numeric_limits<Real>::epsilon());
  if (p.n == 0) {
    checks.Expect(f.rcond == 1);
  } else {
    checks.Expect(std::isfinite(f.rcond) && f.rcond > 0 &&
                  f.rcond <=
                      1 + 64 * p.n * std::numeric_limits<Real>::epsilon());
    if (p.diagonal) {
      DiagonalCondition(p, f, checks);
    } else if (p.n == 1) {
      checks.Expect(std::abs(f.rcond - 1) <=
                    64 * std::numeric_limits<Real>::epsilon());
    }
  }
}

template <typename T>
void Guards(const Profile& p, const Fixture<T>& f, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  Guard(f.original, 1, static_cast<std::size_t>(p.n * (p.n + 1) / 2), T{-19},
        checks);
  Guard(f.factors, 1, static_cast<std::size_t>(p.n * (p.n + 1) / 2), T{-23},
        checks);
  Guard(f.scales, 1, static_cast<std::size_t>(p.n), Real{-59}, checks);
  Guard(f.ferr, 1, static_cast<std::size_t>(p.nrhs), Real{-37}, checks);
  Guard(f.berr, 1, static_cast<std::size_t>(p.nrhs), Real{-41}, checks);
  for (std::size_t slot = 0; slot < f.rhs.size(); ++slot) {
    bool rhs = false;
    bool solution = false;
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        rhs = rhs || slot == Offset(p, p.b_layout, i, j);
        solution = solution || slot == Offset(p, p.x_layout, i, j);
      }
    }
    if (!rhs) {
      checks.Expect(f.rhs[slot] == T{-29});
    }
    if (!solution) {
      checks.Expect(f.solution[slot] == T{-31});
    }
  }
}

template <typename T>
void PlanCounts(const Profile& p, const asc::LapackWorkspacePlan& plan,
                Checks& checks) {
  const auto count =
      p.n == 0 ? 0 : p.n * (p.n + 1) + 2 * p.n * p.nrhs + 5 * p.n + 2 * p.nrhs;
  checks.Expect(plan.regions[kErrors].minimum_entries == count);
  for (std::size_t i = 0; i < plan.regions.size(); ++i) {
    if (i != kErrors) {
      checks.Expect(plan.regions[i].minimum_entries == 0);
    }
  }
}

template <typename T>
void Exercise(const asc::ReferenceLapackProvider& provider, const Profile& p,
              Fixture<T>& f, Checks& checks) {
  const auto before = f;
  const auto views = f.Views(p);
  const auto plan = Take(Query(provider, p, views, f));
  checks.Expect(f.Same(before));
  PlanCounts<T>(p, plan, checks);
  Scratch<T> scratch(plan);
  asc::LapackReport report;
  ++checks.profiles;
  const auto status =
      Execute(provider, p, views, f, plan, scratch.workspace, report);
  if (!status.ok() && checks.failures < 20) {
    std::fprintf(
        stderr,
        "Robust rejection n=%jd rhs=%jd mode=%d diagonal=%d "
        "a00=%La code=%d outcome=%d pivot=%jd\n",
        static_cast<std::intmax_t>(p.n), static_cast<std::intmax_t>(p.nrhs),
        static_cast<int>(p.mode), p.diagonal ? 1 : 0, f.matrix[0].real(),
        static_cast<int>(status.code()), static_cast<int>(report.outcome),
        static_cast<std::intmax_t>(report.diagnostic_index.value_or(-1)));
  }
  checks.Expect(status.ok());
  checks.Expect(
      !report.called_provider && !report.native_info &&
      std::string_view(report.routine.data()).starts_with("asc_robust_"));
  checks.Expect(report.outcome == asc::LapackOutcome::kSuccess &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  const bool generated = p.mode == Mode::kNew || p.mode == Mode::kEquilibrate;
  checks.Expect(generated
                    ? report.factor_family == asc::LapackFactorFamily::kCholesky
                    : !report.factor_family);
  Mutation(p, f, before, checks);
  Reconstruction(p, f, checks);
  f.Quality(p, before, checks);
  if (p.n == 0 || p.nrhs == 0) {
    checks.Expect(SameBytes(f.solution, before.solution));
  }
  Guards(p, f, checks);
  scratch.Guards(plan, checks);
}

template <typename T>
void Workflow(const asc::ReferenceLapackProvider& provider, Profile p,
              int exponent, Checks& checks) {
  Fixture<T> f(p, exponent);
  const auto original_rhs = f.rhs;
  Exercise(provider, p, f, checks);
  if (p.mode == Mode::kNew || p.mode == Mode::kEquilibrate) {
    // Reuse the actual newly computed factor. FACT F requires original B;
    // AP,AFP and the actual scale vector remain exactly as returned.
    const bool scaled = Scaled(p, f);
    p.mode = scaled ? Mode::kScaled : Mode::kSupplied;
    f.rhs = original_rhs;
    Exercise(provider, p, f, checks);
  }
}

template <typename T>
void Profiles(const asc::ReferenceLapackProvider& provider, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array exponents{
      -20, 0, 20, (std::numeric_limits<Real>::min_exponent - 1) / 2,
      (std::numeric_limits<Real>::max_exponent - 12) / 2};
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kRow, kColumn}) {
      for (const auto af : {kRow, kColumn}) {
        for (const auto b : {kRow, kColumn}) {
          for (const auto x : {kRow, kColumn}) {
            for (const asc::extent_t n : {0, 1, 2, 3, 5}) {
              for (const asc::extent_t nrhs : {0, 1, 3}) {
                for (const auto mode : {Mode::kNew, Mode::kEquilibrate,
                                        Mode::kSupplied, Mode::kScaled}) {
                  const Profile p{n, nrhs, triangle, a, af, b, x, mode};
                  for (int exponent : exponents) {
                    Workflow<T>(provider, p, exponent, checks);
                  }
                  if (mode == Mode::kEquilibrate) {
                    Workflow<T>(provider, p,
                                std::numeric_limits<Real>::min_exponent + 3,
                                checks);
                    Workflow<T>(provider, p,
                                std::numeric_limits<Real>::max_exponent - 12,
                                checks);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
template <typename T>
void DiagonalProfiles(const asc::ReferenceLapackProvider& provider,
                      Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kRow, kColumn}) {
      for (const auto af : {kRow, kColumn}) {
        for (const auto b : {kRow, kColumn}) {
          for (const auto x : {kRow, kColumn}) {
            for (const asc::extent_t n : {1, 2, 5}) {
              for (const asc::extent_t nrhs : {0, 2}) {
                for (const auto mode : {Mode::kNew, Mode::kEquilibrate,
                                        Mode::kSupplied, Mode::kScaled}) {
                  const Profile p{n, nrhs, triangle, a, af, b, x, mode, true};
                  for (int exponent : {-20, 0, 20}) {
                    Workflow<T>(provider, p, exponent, checks);
                  }
                  if (mode == Mode::kEquilibrate) {
                    Workflow<T>(provider, p,
                                std::numeric_limits<Real>::min_exponent + 3,
                                checks);
                    Workflow<T>(provider, p,
                                std::numeric_limits<Real>::max_exponent - 12,
                                checks);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
template <typename T>
void OrdinaryProfiles(const asc::ReferenceLapackProvider& provider,
                      Checks& checks) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kRow, kColumn}) {
      const auto other = layout == kRow ? kColumn : kRow;
      for (const auto mode :
           {Mode::kNew, Mode::kEquilibrate, Mode::kSupplied, Mode::kScaled}) {
        for (const bool diagonal : {false, true}) {
          const Profile p{3,     2,      triangle, layout,  other,
                          other, layout, mode,     diagonal};
          Workflow<T>(provider, p, 0, checks);
        }
      }
    }
  }
}

template <typename T>
class ReadObservation {
 public:
  void Begin(Fixture<T>& fixture, const Profile& profile, bool metadata) {
#if defined(ASC_ROBUST_PPSVX_LOAD_OBSERVATION)
    Set(0, fixture.original);
    Set(1, fixture.factors);
    Set(2, fixture.rhs);
    Set(3, fixture.solution);
    Set(4, fixture.scales);
    Set(5, fixture.ferr);
    Set(6, fixture.berr);
    Set(7, std::span(&fixture.rcond, 1));
    if (!metadata) {
      regions_[0] = {};
      regions_[2] = {};
      if (profile.mode != Mode::kEquilibrate && profile.mode != Mode::kScaled) {
        regions_[4] = {};
      }
    }
    asc_display_observation::Start(regions_);
#else
    static_cast<void>(fixture);
    static_cast<void>(profile);
    static_cast<void>(metadata);
#endif
  }

  void End(const Profile& profile, bool metadata, Checks& checks) {
#if defined(ASC_ROBUST_PPSVX_LOAD_OBSERVATION)
    asc_display_observation::Stop();
    for (std::size_t i = 0; i < regions_.size(); ++i) {
      const bool factor = i == 1 && (profile.mode == Mode::kSupplied ||
                                     profile.mode == Mode::kScaled);
      const bool scales = i == 4 && profile.mode == Mode::kScaled;
      const bool expected = !metadata && profile.n > 0 && (factor || scales);
      checks.Expect(Read(i) == expected);
    }
#else
    static_cast<void>(profile);
    static_cast<void>(metadata);
    static_cast<void>(checks);
#endif
  }

  void Negative(std::size_t index, Checks& checks) {
#if defined(ASC_ROBUST_PPSVX_LOAD_OBSERVATION)
    using Real = asc::DenseBlasRealType<T>;
    std::size_t offset = index == 1 || index == 3 ? sizeof(T) : sizeof(Real);
    if (index == 7) {
      offset = 0;
    }
    const volatile std::byte* pointer = regions_[index].backing.data() + offset;
    const std::byte value = *pointer;
    static_cast<void>(value);
    asc_display_observation::Stop();
    checks.Expect(Read(index));
#else
    static_cast<void>(index);
    static_cast<void>(checks);
#endif
  }

 private:
#if defined(ASC_ROBUST_PPSVX_LOAD_OBSERVATION)
  std::array<std::array<std::uint32_t, 1024>, 8> counts_{};
  std::array<asc_display_observation::Region, 8> regions_{};

  template <typename Buffer>
  void Set(std::size_t index, const Buffer& buffer) {
    const auto bytes = std::as_bytes(std::span(buffer));
    if (bytes.size() > counts_[index].size()) {
      std::abort();
    }
    regions_[index] = {bytes, std::span(counts_[index]).first(bytes.size())};
  }

  [[nodiscard]] bool Read(std::size_t index) const {
    const auto values = regions_[index].reads;
    return std::any_of(values.begin(), values.end(),
                       [](std::uint32_t value) { return value != 0; });
  }
#endif
};

template <typename T>
void RejectWorkspace(const asc::ReferenceLapackProvider& provider,
                     const Profile& profile, Checks& checks) {
  Fixture<T> fixture(profile, 0);
  const auto before = fixture;
  const auto views = fixture.Views(profile);
  ReadObservation<T> observation;
  observation.Begin(fixture, profile, true);
  const auto queried = Query(provider, profile, views, fixture);
  observation.End(profile, true, checks);
  const auto plan = Take(queried);
  checks.Expect(fixture.Same(before));
  Scratch<T> scratch(plan);
  const auto untouched = scratch.storage;
  for (int scenario = 0; scenario < 4; ++scenario) {
    auto supplied = plan;
    auto execution_profile = profile;
    auto workspace = scratch.workspace;
    const auto region = workspace.regions[kErrors];
    if ((scenario == 1 || scenario == 2) && region.size() == 0) {
      continue;
    }
    if (scenario == 0) {
      ++supplied.regions[kErrors].minimum_entries;
    } else if (scenario == 1) {
      workspace.regions[kErrors] = {region.data(), region.size() - 1, kHost};
    } else if (scenario == 2) {
      auto* bytes = static_cast<std::byte*>(region.data());
      workspace.regions[kErrors] = {bytes + 1, region.size(), kHost};
    } else {
      execution_profile.triangle = profile.triangle == kUpper ? kLower : kUpper;
    }
    asc::LapackReport report;
    report.native_info = 71;
    observation.Begin(fixture, profile, true);
    const auto status = Execute(provider, execution_profile, views, fixture,
                                supplied, workspace, report);
    observation.End(profile, true, checks);
    checks.Expect(!status.ok() && !report.native_info &&
                  !report.called_provider && fixture.Same(before) &&
                  scratch.storage == untouched);
  }
  asc::LapackReport report;
  observation.Begin(fixture, profile, false);
#if defined(ASC_ROBUST_PPSVX_ALLOCATION)
  const asc_dense_test::AllocationProbe allocations;
  asc_lapack_test::BeginAllocationAudit();
#endif
  const auto status = Execute(provider, profile, views, fixture, plan,
                              scratch.workspace, report);
#if defined(ASC_ROBUST_PPSVX_ALLOCATION)
  const auto c_allocations = asc_lapack_test::EndAllocationAudit();
  checks.Expect(c_allocations == 0 && allocations.count() == 0);
#endif
  observation.End(profile, false, checks);
  ++checks.profiles;
  checks.Expect(status.ok() && !report.called_provider && !report.native_info);
  fixture.Quality(profile, before, checks);
  Mutation(profile, fixture, before, checks);
  Reconstruction(profile, fixture, checks);
  Guards(profile, fixture, checks);
  scratch.Guards(plan, checks);
}

template <typename T>
void PublicationLoss(const asc::ReferenceLapackProvider& provider,
                     Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const Real tiny = std::numeric_limits<Real>::denorm_min();
  const Real huge = std::numeric_limits<Real>::max();
  // Three distinct publication failures: a nonzero solution below denorm_min,
  // an overflowing solution, and excessive relative subnormal rounding.
  const std::array coefficients{huge, Real{0.5}, Real{2}};
  const std::array right_sides{tiny, huge, 3 * tiny};
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kRow, kColumn}) {
      for (const auto mode :
           {Mode::kNew, Mode::kEquilibrate, Mode::kSupplied, Mode::kScaled}) {
        for (std::size_t scenario = 0; scenario < coefficients.size();
             ++scenario) {
          const Profile profile{1,      1,      triangle, layout,
                                layout, layout, layout,   mode};
          Fixture<T> fixture(profile, 0);
          fixture.original[1] = T{coefficients[scenario]};
          fixture.factors[1] = T{std::sqrt(coefficients[scenario])};
          fixture.scales[1] = Real{1};
          fixture.rhs[1] = T{right_sides[scenario]};
          const auto before = fixture;
          const auto views = fixture.Views(profile);
          const auto plan = Take(Query(provider, profile, views, fixture));
          Scratch<T> scratch(plan);
          asc::LapackReport report;
          const auto status = Execute(provider, profile, views, fixture, plan,
                                      scratch.workspace, report);
          // Exact real arithmetic gives 0<tiny/huge<tiny, 2*huge>huge,
          // and 3*tiny/2 halfway between adjacent subnormal values. No
          // unrepresentable quotient is evaluated in the independent oracle.
          checks.Expect(tiny > 0 && huge > 1);
          checks.Expect(
              status.code() == asc::ErrorCode::kOverflow &&
              report.outcome == asc::LapackOutcome::kAccuracyWarning &&
              fixture.Same(before) && !report.native_info &&
              !report.called_provider && !report.factor_family &&
              report.output_validity == asc::LapackOutputValidity::kUnchanged);
          scratch.Guards(plan, checks);
        }
      }
    }
  }
}

template <typename T>
void AccuracyWarning(const asc::ReferenceLapackProvider& provider,
                     Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  // This SPD correlation matrix has eigenvalues 1+t and 1-t, giving
  // reciprocal condition approximately epsilon/4. No equilibration is needed.
  const Real t = 1 - std::numeric_limits<Real>::epsilon() / 2;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto mode :
         {Mode::kNew, Mode::kEquilibrate, Mode::kSupplied, Mode::kScaled}) {
      const Profile profile{2,       1,       triangle, kColumn,
                            kColumn, kColumn, kColumn,  mode};
      Fixture<T> fixture(profile, 0);
      fixture.original[1] = T{1};
      fixture.original[2] = T{t};
      fixture.original[3] = T{1};
      fixture.factors[1] = T{1};
      fixture.factors[2] = T{t};
      fixture.factors[3] = T{std::sqrt(1 - t * t)};
      fixture.scales[1] = fixture.scales[2] = Real{1};
      fixture.rhs[1] = fixture.rhs[2] = T{1 + t};
      const auto views = fixture.Views(profile);
      const auto plan = Take(Query(provider, profile, views, fixture));
      Scratch<T> scratch(plan);
      asc::LapackReport report;
      const auto status = Execute(provider, profile, views, fixture, plan,
                                  scratch.workspace, report);
      checks.Expect(status.code() == asc::ErrorCode::kNumerical &&
                    report.outcome == asc::LapackOutcome::kAccuracyWarning &&
                    report.output_validity ==
                        asc::LapackOutputValidity::kDocumentedPartial &&
                    !report.native_info && !report.called_provider &&
                    !report.factor_family && !report.diagnostic_index);
      checks.Expect(std::isfinite(fixture.rcond) && fixture.rcond > 0 &&
                    fixture.rcond < std::numeric_limits<Real>::epsilon());
      checks.Expect(std::isfinite(fixture.ferr[1]) && fixture.ferr[1] >= 0 &&
                    std::isfinite(fixture.berr[1]) && fixture.berr[1] >= 0 &&
                    fixture.berr[1] <= 1);
      scratch.Guards(plan, checks);
    }
  }
}

template <typename T>
void MixedScale(const asc::ReferenceLapackProvider& provider, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kRow, kColumn}) {
      Profile profile{3,      1,      triangle,           layout, layout,
                      layout, layout, Mode::kEquilibrate, true,   true};
      Fixture<T> fixture(profile, 0);
      fixture.matrix = {};
      const std::array diagonal{8 * std::numeric_limits<Real>::denorm_min(),
                                Real{1}, std::numeric_limits<Real>::max() / 4};
      for (asc::extent_t i = 0; i < 3; ++i) {
        fixture.matrix[static_cast<std::size_t>(i * 3 + i)] =
            diagonal[static_cast<std::size_t>(i)];
        fixture.rhs[Offset(profile, layout, i, 0)] = Narrow<T>(
            static_cast<long double>(diagonal[static_cast<std::size_t>(i)]) *
            Exact<T>(i, 0));
      }
      fixture.InitializePacked(profile, 1);
      const auto rhs = fixture.rhs;
      Exercise(provider, profile, fixture, checks);
      checks.Expect(fixture.equilibration ==
                    asc::LapackCholeskyEquilibration::kDiagonal);
      profile.mode = Mode::kScaled;
      fixture.rhs = rhs;
      Exercise(provider, profile, fixture, checks);
    }
  }
}

template <typename T>
void NonfiniteFixture(const asc::ReferenceLapackProvider& provider,
                      Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  // Preserve the initially overflowing generalization fixture as a rejection
  // regression. Its supplied D*A*D coefficient is nonfinite, not finite SPD.
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kRow, kColumn}) {
      for (const auto af : {kRow, kColumn}) {
        for (const auto b : {kRow, kColumn}) {
          for (const auto x : {kRow, kColumn}) {
            for (const asc::extent_t nrhs : {0, 1, 3}) {
              const Profile profile{5,  nrhs, triangle, a,
                                    af, b,    x,        Mode::kScaled};
              Fixture<T> fixture(profile,
                                 std::numeric_limits<Real>::max_exponent - 8);
              const auto before = fixture;
              const auto views = fixture.Views(profile);
              const auto plan = Take(Query(provider, profile, views, fixture));
              Scratch<T> scratch(plan);
              asc::LapackReport report;
              const auto status = Execute(provider, profile, views, fixture,
                                          plan, scratch.workspace, report);
              checks.Expect(status.code() == asc::ErrorCode::kInvalidArgument &&
                            fixture.Same(before) && !report.called_provider &&
                            !report.native_info && !report.factor_family);
              scratch.Guards(plan, checks);
            }
          }
        }
      }
    }
  }
}

template <typename T>
void SafetyProfiles(const asc::ReferenceLapackProvider& provider,
                    Checks& checks) {
  const Profile control{3, 2, kLower, kRow, kColumn, kColumn, kRow, Mode::kNew};
  Fixture<T> fixture(control, 0);
  ReadObservation<T> observation;
  for (const std::size_t index : {1U, 3U, 5U, 6U, 7U}) {
    observation.Begin(fixture, control, false);
    observation.Negative(index, checks);
  }
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kRow, kColumn}) {
      for (const auto af : {kRow, kColumn}) {
        for (const auto b : {kRow, kColumn}) {
          for (const auto x : {kRow, kColumn}) {
            for (const asc::extent_t n : {0, 3}) {
              for (const asc::extent_t nrhs : {0, 2}) {
                for (const auto mode : {Mode::kNew, Mode::kEquilibrate,
                                        Mode::kSupplied, Mode::kScaled}) {
                  const Profile profile{n, nrhs, triangle, a, af, b, x, mode};
                  RejectWorkspace<T>(provider, profile, checks);
                }
              }
            }
          }
        }
      }
    }
  }
  NonfiniteFixture<T>(provider, checks);
  PublicationLoss<T>(provider, checks);
  AccuracyWarning<T>(provider, checks);
  MixedScale<T>(provider, checks);
}

template <typename T>
void GeneralProfiles(const asc::ReferenceLapackProvider& provider,
                     Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array exponents{std::numeric_limits<Real>::min_exponent -
                                 std::numeric_limits<Real>::digits + 9,
                             std::numeric_limits<Real>::min_exponent + 3, -17,
                             0, std::numeric_limits<Real>::max_exponent - 12};
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto a : {kRow, kColumn}) {
      for (const auto af : {kRow, kColumn}) {
        for (const auto b : {kRow, kColumn}) {
          for (const auto x : {kRow, kColumn}) {
            for (const asc::extent_t n : {0, 1, 3, 5}) {
              for (const asc::extent_t nrhs : {0, 1, 3}) {
                for (const auto mode : {Mode::kNew, Mode::kEquilibrate,
                                        Mode::kSupplied, Mode::kScaled}) {
                  const Profile profile{n, nrhs, triangle, a,     af,
                                        b, x,    mode,     false, true};
                  for (const int exponent : exponents) {
                    Workflow<T>(provider, profile, exponent, checks);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

template <typename T>
void DiagnosticIsolation(const asc::ReferenceLapackProvider& provider,
                         Checks& checks) {
  const Profile p{3, 2, kUpper, kColumn, kRow, kRow, kColumn, Mode::kNew};
  Fixture<T> fixture(p, 0);
  fixture.original[1] = T{-1};
  const auto before = fixture;
  const auto views = fixture.Views(p);
  const auto plan = Take(Query(provider, p, views, fixture));
  Scratch<T> scratch(plan);
  asc::LapackReport report;
  const auto status =
      Execute(provider, p, views, fixture, plan, scratch.workspace, report);
  checks.Expect(!status.ok() && !report.called_provider &&
                !report.native_info && report.diagnostic_index == 0 &&
                report.outcome == asc::LapackOutcome::kNotPositiveDefinite &&
                !report.factor_family && fixture.Same(before) &&
                SameBytes(fixture.solution, before.solution) &&
                SameBytes(fixture.ferr, before.ferr) &&
                SameBytes(fixture.berr, before.berr));
  // A valid call in this worker must retain its own clean diagnostics while
  // other workers can be in either the successful or failed provider path.
  Workflow<T>(provider, p, 0, checks);
}

template <typename T>
void Worker(std::barrier<>& start, Checks& checks) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const int rounding = std::fegetround();
  start.arrive_and_wait();
  for (int repeat = 0; repeat < 8; ++repeat) {
    OrdinaryProfiles<T>(provider, checks);
    DiagnosticIsolation<T>(provider, checks);
  }
  checks.Expect(std::fegetround() == rounding);
}

void ConcurrentProfiles(Checks& checks) {
  std::barrier start(4);
  std::array<Checks, 4> local{};
  std::array workers{
      std::thread([&] { Worker<float>(start, local[0]); }),
      std::thread([&] { Worker<double>(start, local[1]); }),
      std::thread([&] { Worker<std::complex<float>>(start, local[2]); }),
      std::thread([&] { Worker<std::complex<double>>(start, local[3]); })};
  for (auto& worker : workers) {
    worker.join();
  }
  for (const auto& result : local) {
    checks.profiles += result.profiles;
    checks.checks += result.checks;
    checks.failures += result.failures;
  }
}

}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Checks checks;
  const int rounding = std::fegetround();
  if (argc == 2) {
    const std::string_view mode(argv[1]);
    if (mode == "--ordinary") {
      OrdinaryProfiles<float>(provider, checks);
      OrdinaryProfiles<double>(provider, checks);
      OrdinaryProfiles<std::complex<float>>(provider, checks);
      OrdinaryProfiles<std::complex<double>>(provider, checks);
    } else if (mode == "--safety") {
      SafetyProfiles<float>(provider, checks);
      SafetyProfiles<double>(provider, checks);
      SafetyProfiles<std::complex<float>>(provider, checks);
      SafetyProfiles<std::complex<double>>(provider, checks);
    } else if (mode == "--general-s") {
      GeneralProfiles<float>(provider, checks);
    } else if (mode == "--general-d") {
      GeneralProfiles<double>(provider, checks);
    } else if (mode == "--general-c") {
      GeneralProfiles<std::complex<float>>(provider, checks);
    } else if (mode == "--general-z") {
      GeneralProfiles<std::complex<double>>(provider, checks);
    } else if (mode == "--general") {
      GeneralProfiles<float>(provider, checks);
      GeneralProfiles<double>(provider, checks);
      GeneralProfiles<std::complex<float>>(provider, checks);
      GeneralProfiles<std::complex<double>>(provider, checks);
    } else if (mode == "--concurrent") {
      ConcurrentProfiles(checks);
    } else {
      return 2;
    }
    checks.Expect(std::fegetround() == rounding);
    std::printf(
        "PPSVX public family %.*s: %zu profiles, %zu checks, "
        "%zu failures; independent provider contexts and workspaces\n",
        static_cast<int>(mode.size()), mode.data(), checks.profiles,
        checks.checks, checks.failures);
    return checks.failures == 0 ? 0 : 1;
  }
  Profiles<float>(provider, checks);
  Profiles<double>(provider, checks);
  Profiles<std::complex<float>>(provider, checks);
  Profiles<std::complex<double>>(provider, checks);
  DiagonalProfiles<float>(provider, checks);
  DiagonalProfiles<double>(provider, checks);
  DiagonalProfiles<std::complex<float>>(provider, checks);
  DiagonalProfiles<std::complex<double>>(provider, checks);
  checks.Expect(std::fegetround() == rounding);
  std::printf(
      "Packed Cholesky expert: %zu profiles, %zu checks, %zu failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
