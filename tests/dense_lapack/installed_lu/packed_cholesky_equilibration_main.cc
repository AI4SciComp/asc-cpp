#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_equilibration.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;

struct Checks {
  std::size_t profiles = 0;
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      if (failures < 15) {
        std::fprintf(stderr, "Packed equilibration failure at check %zu\n",
                     checks);
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

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 32> matrix;
  std::array<Real, 10> scales;
  std::array<Real, 3> condition{Real{-29}, Real{-29}, Real{-29}};
  std::array<Real, 3> maximum{Real{-31}, Real{-31}, Real{-31}};
  std::array<Real, 5> diagonal{};
  Fixture(const Profile& p, int exponent, bool ignored_nan) {
    matrix.fill(T{-19});
    scales.fill(Real{-23});
    for (asc::extent_t i = 0; i < p.n; ++i) {
      const auto d = static_cast<Real>(i + 1);
      diagonal[static_cast<std::size_t>(i)] = std::ldexp(d * d, exponent);
    }
    SetMatrix(p, ignored_nan);
  }
  void SetMatrix(const Profile& p, bool ignored_nan) {
    const auto nan = std::numeric_limits<Real>::quiet_NaN();
    Slots(p, [&](std::size_t offset, auto i, auto j) {
      const auto real = i == j ? diagonal[static_cast<std::size_t>(i)] : nan;
      if constexpr (asc::DenseBlasComplex<T>) {
        matrix[offset] = {real, ignored_nan ? nan : Real{37}};
      } else {
        matrix[offset] = real;
      }
    });
  }
  auto A(const Profile& p) {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        matrix.data() + 1, p.n, p.layout,
        {matrix.data(), sizeof(matrix), kHost}));
  }
  auto S(const Profile& p) {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        scales.data() + 1, p.n, 1, {scales.data(), sizeof(scales), kHost}));
  }
  void Guards(const Profile& p, const std::array<T, 32>& old, Checks& checks) {
    checks.Expect(SameBytes(matrix, old));
    for (std::size_t i = 0; i < scales.size(); ++i) {
      if (i == 0 || i > static_cast<std::size_t>(p.n)) {
        checks.Expect(scales[i] == Real{-23});
      }
    }
    checks.Expect(condition.front() == Real{-29} &&
                  condition.back() == Real{-29});
    checks.Expect(maximum.front() == Real{-31} && maximum.back() == Real{-31});
  }
};

template <typename Real>
void Relative(long double actual, long double expected, Checks& checks) {
  // Analytic positive dyadic diagonals, at most five entries; eight rounding
  // units cover square-root, reciprocal and ratio. No additive denominator.
  const long double tolerance = 8 * std::numeric_limits<Real>::epsilon();
  checks.Expect(std::isfinite(actual));
  checks.Expect(expected == 0
                    ? actual == 0
                    : std::abs(actual - expected) / std::abs(expected) <=
                          tolerance);
}

template <typename T>
void Success(const asc::ReferenceLapackProvider& provider, const Profile& p,
             int exponent, bool ignored_nan, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> f(p, exponent, ignored_nan);
  const auto old = f.matrix;
  const auto a = f.A(p);
  const auto s = f.S(p);
  const auto plan = Take(asc::QueryPpequWorkspace(
      provider, p.triangle, a, s, f.condition[1], f.maximum[1]));
  for (const auto& region : plan.regions) {
    checks.Expect(region.minimum_entries == 0 && region.preferred_entries == 0);
  }
  for (int repeat = 0; repeat < 2; ++repeat) {
    ++checks.profiles;
    asc::LapackReport report;
    const auto status = asc::Ppequ(provider, p.triangle, a, s, f.condition[1],
                                   f.maximum[1], plan, {}, report);
    checks.Expect(status.ok());
    checks.Expect(report.called_provider == (p.n != 0) &&
                  (p.n == 0 ? !report.native_info : report.native_info == 0));
    checks.Expect(report.outcome == asc::LapackOutcome::kSuccess &&
                  report.output_validity ==
                      asc::LapackOutputValidity::kComplete);
    const long double unit = std::ldexp(1.0L, -exponent / 2);
    for (asc::extent_t i = 0; i < p.n; ++i) {
      const auto index = static_cast<std::size_t>(i);
      const long double actual = f.scales[index + 1];
      Relative<Real>(actual, unit / (i + 1), checks);
      // Independently check that applying each returned scale normalizes its
      // diagonal. Widen before products so verifier arithmetic stays finite.
      Relative<Real>(
          actual * actual * static_cast<long double>(f.diagonal[index]), 1,
          checks);
    }
    Relative<Real>(f.condition[1], p.n == 0 ? 1 : 1.0L / p.n, checks);
    const long double maximum =
        std::ldexp(static_cast<long double>(p.n * p.n), exponent);
    Relative<Real>(f.maximum[1], maximum, checks);
    f.Guards(p, old, checks);
  }
}

template <typename T>
void Nonpositive(const asc::ReferenceLapackProvider& provider, const Profile& p,
                 int exponent, bool ignored_nan, int failure, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> f(p, exponent, ignored_nan);
  const auto index = failure % 2 == 0 ? 0 : p.n - 1;
  f.diagonal[static_cast<std::size_t>(index)] =
      failure < 2 ? Real{} : -std::ldexp(Real{1}, exponent);
  if (failure == 4) {
    f.diagonal[static_cast<std::size_t>(p.n - 1)] =
        -std::ldexp(Real{2}, exponent);
  }
  f.SetMatrix(p, ignored_nan);
  const auto old = f.matrix;
  const auto a = f.A(p);
  const auto s = f.S(p);
  const auto plan = Take(asc::QueryPpequWorkspace(
      provider, p.triangle, a, s, f.condition[1], f.maximum[1]));
  ++checks.profiles;
  asc::LapackReport report;
  const auto status = asc::Ppequ(provider, p.triangle, a, s, f.condition[1],
                                 f.maximum[1], plan, {}, report);
  checks.Expect(status.code() == asc::ErrorCode::kNumerical &&
                report.called_provider);
  checks.Expect(report.native_info == index + 1 &&
                report.diagnostic_index == index);
  checks.Expect(report.outcome == asc::LapackOutcome::kNotPositiveDefinite &&
                report.output_validity ==
                    asc::LapackOutputValidity::kDocumentedPartial);
  Real maximum = f.diagonal.front();
  for (asc::extent_t i = 0; i < p.n; ++i) {
    const auto k = static_cast<std::size_t>(i);
    checks.Expect(f.scales[k + 1] == f.diagonal[k]);
    maximum = std::max(maximum, f.diagonal[k]);
  }
  checks.Expect(f.maximum[1] == maximum && f.condition[1] == Real{-29});
  f.Guards(p, old, checks);
}

template <typename T>
void Nonfinite(const asc::ReferenceLapackProvider& provider, const Profile& p,
               int position, bool nan, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> f(p, 0, true);
  f.diagonal[static_cast<std::size_t>(position)] =
      nan ? std::numeric_limits<Real>::quiet_NaN()
          : std::numeric_limits<Real>::infinity();
  f.SetMatrix(p, true);
  const auto old = f.matrix;
  const auto a = f.A(p);
  const auto s = f.S(p);
  const auto plan = Take(asc::QueryPpequWorkspace(
      provider, p.triangle, a, s, f.condition[1], f.maximum[1]));
  ++checks.profiles;
  asc::LapackReport report;
  const auto status = asc::Ppequ(provider, p.triangle, a, s, f.condition[1],
                                 f.maximum[1], plan, {}, report);
  checks.Expect(status.code() == asc::ErrorCode::kNumerical &&
                report.called_provider);
  checks.Expect(report.native_info == 0 &&
                report.outcome == asc::LapackOutcome::kAccuracyWarning);
  checks.Expect(report.output_validity ==
                asc::LapackOutputValidity::kDocumentedPartial);
  const auto value = f.scales[static_cast<std::size_t>(position) + 1];
  checks.Expect(nan ? std::isnan(value) : value == 0);
  f.Guards(p, old, checks);
}

template <typename T>
void Run(const asc::ReferenceLapackProvider& provider, Checks& checks) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array exponents{-40,
                             -2,
                             0,
                             20,
                             40,
                             std::numeric_limits<Real>::min_exponent - 1,
                             std::numeric_limits<Real>::max_exponent - 10};
  for (const auto n : {0, 1, 2, 3, 5}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kRow, kColumn}) {
        const Profile p{n, triangle, layout};
        for (const bool ignored_nan : {false, true}) {
          if (!asc::DenseBlasComplex<T> && ignored_nan) {
            continue;
          }
          for (const auto exponent : exponents) {
            Success<T>(provider, p, exponent, ignored_nan, checks);
            if (n != 0) {
              for (int failure = 0; failure < 5; ++failure) {
                Nonpositive<T>(provider, p, exponent, ignored_nan, failure,
                               checks);
              }
            }
          }
        }
        if (n != 0) {
          for (const auto position : {0, n - 1}) {
            for (const bool nan : {false, true}) {
              Nonfinite<T>(provider, p, position, nan, checks);
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
  const auto context = asc::ExecutionContext::Serial();
  const auto provider = Take(asc::ReferenceLapackProvider::Create(context));
  Checks checks;
  Run<float>(provider, checks);
  Run<double>(provider, checks);
  Run<std::complex<float>>(provider, checks);
  Run<std::complex<double>>(provider, checks);
  std::printf(
      "Packed Cholesky equilibration: %zu profiles, %zu checks, %zu failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
