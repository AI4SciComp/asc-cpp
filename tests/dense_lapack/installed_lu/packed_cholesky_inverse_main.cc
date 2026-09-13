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
#include "asc/dense/providers/lapack_cholesky_packed_inverse.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Wide;
using installed_internal::Widen;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

struct Checks {
  std::size_t profiles = 0;
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      if (failures < 15) {
        std::fprintf(stderr, "Packed Cholesky inverse failure at check %zu\n",
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

template <typename T>
Wide Q() {
  return {0.25L, asc::DenseBlasComplex<T> ? 0.125L : 0};
}

long double Diagonal(asc::extent_t i) {
  return 2 + static_cast<long double>(i) / 2;
}

template <typename T>
Wide Lower(asc::extent_t i, asc::extent_t j) {
  if (i == j) {
    return {Diagonal(i), 0};
  }
  return i == j + 1 ? Diagonal(i) * Q<T>() : Wide{};
}

template <typename T>
Wide Matrix(asc::extent_t n, asc::extent_t i, asc::extent_t j) {
  Wide result{};
  for (asc::extent_t k = 0; k < n; ++k) {
    result += Lower<T>(i, k) * std::conj(Lower<T>(j, k));
  }
  return result;
}

// L=D*(I+q*S), with S the lower shift and S^N=0. Its independently known
// inverse is the finite geometric series sum((-q*S)^k)*D^-1. No provider
// factorization/solve/inverse routine constructs the expected answer.
template <typename T>
Wide LowerInverse(asc::extent_t i, asc::extent_t j) {
  if (i < j) {
    return {};
  }
  Wide result{1, 0};
  for (asc::extent_t k = j; k < i; ++k) {
    result *= -Q<T>();
  }
  return result / Diagonal(j);
}

template <typename T>
Wide KnownInverse(asc::extent_t n, asc::extent_t i, asc::extent_t j) {
  Wide result{};
  for (asc::extent_t k = 0; k < n; ++k) {
    result += std::conj(LowerInverse<T>(k, i)) * LowerInverse<T>(k, j);
  }
  return result;
}

template <typename T>
T Narrow(Wide value) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(value.real()), static_cast<Real>(value.imag())};
  } else {
    return static_cast<T>(value.real());
  }
}

template <typename T>
struct Fixture {
  std::array<T, 32> factors;
  std::array<T, 32> packing;
  Fixture(const Profile& p, int exponent, asc::extent_t singular) {
    factors.fill(T{-19});
    packing.fill(T{-23});
    const auto scale = std::ldexp(1.0L, exponent / 2);
    Slots(p, [&](std::size_t offset, auto i, auto j) {
      const auto value =
          p.triangle == kLower ? Lower<T>(i, j) : std::conj(Lower<T>(j, i));
      factors[offset] =
          i == j && i == singular ? T{} : Narrow<T>(value * scale);
    });
  }
  auto A(const Profile& p) {
    return Take(asc::DenseBlasPackedMatrixView<T>::Create(
        factors.data() + 1, p.n, p.layout,
        {factors.data(), sizeof(factors), kHost}));
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    const auto count = plan.regions[kPacking].minimum_entries;
    if (count != 0) {
      workspace.regions[kPacking] = {
          packing.data() + 1, static_cast<std::size_t>(count) * sizeof(T),
          kHost};
    }
    return workspace;
  }
};

template <typename T>
void Equation(Checks& checks, const Fixture<T>& sample, const Profile& p,
              int exponent) {
  std::array<Wide, 25> inverse{};
  Slots(p, [&](std::size_t offset, auto i, auto j) {
    const auto value = Widen(sample.factors[offset]);
    checks.Expect(std::isfinite(std::abs(value)));
    if (i == j) {
      checks.Expect(value.real() > 0 && value.imag() == 0);
    }
    inverse[static_cast<std::size_t>(i * p.n + j)] = value;
    inverse[static_cast<std::size_t>(j * p.n + i)] = std::conj(value);
  });
  const auto scale = std::ldexp(1.0L, exponent);
  long double norm_a = 0;
  long double norm_inverse = 0;
  long double expected_norm = 0;
  long double residual = 0;
  long double error = 0;
  for (asc::extent_t i = 0; i < p.n; ++i) {
    long double a_row = 0;
    long double x_row = 0;
    long double expected_row = 0;
    long double left_row = 0;
    long double right_row = 0;
    long double error_row = 0;
    for (asc::extent_t j = 0; j < p.n; ++j) {
      Wide left{};
      Wide right{};
      for (asc::extent_t k = 0; k < p.n; ++k) {
        left += Matrix<T>(p.n, i, k) * scale *
                inverse[static_cast<std::size_t>(k * p.n + j)];
        right += inverse[static_cast<std::size_t>(i * p.n + k)] *
                 Matrix<T>(p.n, k, j) * scale;
      }
      const auto expected = KnownInverse<T>(p.n, i, j) / scale;
      a_row += std::abs(Matrix<T>(p.n, i, j) * scale);
      x_row += std::abs(inverse[static_cast<std::size_t>(i * p.n + j)]);
      expected_row += std::abs(expected);
      error_row +=
          std::abs(inverse[static_cast<std::size_t>(i * p.n + j)] - expected);
      left_row += std::abs(left - (i == j ? Wide{1, 0} : Wide{}));
      right_row += std::abs(right - (i == j ? Wide{1, 0} : Wide{}));
    }
    norm_a = std::max(norm_a, a_row);
    norm_inverse = std::max(norm_inverse, x_row);
    expected_norm = std::max(expected_norm, expected_row);
    error = std::max(error, error_row);
    residual = std::max({residual, left_row, right_row});
  }
  // All admitted nonempty fixtures are well conditioned. The n-scaled bound
  // uses each scalar's epsilon; wider verifier products avoid scale overflow.
  const long double bound =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() * p.n;
  const long double denominator = norm_a * norm_inverse;
  checks.Expect(denominator > 0 && std::isfinite(denominator));
  checks.Expect(std::isfinite(residual) && residual / denominator <= bound);
  checks.Expect(expected_norm > 0 && std::isfinite(expected_norm));
  checks.Expect(std::isfinite(error) && error / expected_norm <= bound);
}

template <typename T>
void Run(Checks& checks, const asc::ReferenceLapackProvider& provider,
         const Profile& p, int exponent, asc::extent_t singular) {
  Fixture<T> sample(p, exponent, singular);
  const auto old = sample.factors;
  const auto a = sample.A(p);
  const auto plan = Take(asc::QueryPptriWorkspace(provider, p.triangle, a));
  const auto entries = p.layout == kRow ? p.n * (p.n + 1) / 2 : 0;
  checks.Expect(plan.regions[kPacking].minimum_entries == entries);
  const auto workspace = sample.Workspace(plan);
  asc::LapackReport report;
  const auto status =
      asc::Pptri(provider, p.triangle, a, plan, workspace, report);
  if (p.n == 0) {
    checks.Expect(status.ok() && !report.called_provider &&
                  !report.native_info);
    checks.Expect(report.output_validity ==
                  asc::LapackOutputValidity::kComplete);
    checks.Expect(sample.factors == old);
  } else if (singular >= 0) {
    checks.Expect(status.code() == asc::ErrorCode::kNumerical);
    checks.Expect(report.called_provider && report.native_info == singular + 1);
    checks.Expect(report.diagnostic_index == singular);
    checks.Expect(report.outcome == asc::LapackOutcome::kSingular &&
                  report.output_validity ==
                      asc::LapackOutputValidity::kUnchanged);
    checks.Expect(sample.factors == old);
  } else {
    checks.Expect(installed_internal::Succeeded(status, report));
    Equation(checks, sample, p, exponent);
  }
  const auto packed = static_cast<std::size_t>(p.n * (p.n + 1) / 2);
  for (std::size_t i = 0; i < sample.factors.size(); ++i) {
    if (i == 0 || i > packed) {
      checks.Expect(sample.factors[i] == old[i]);
    }
    if (i == 0 || i > static_cast<std::size_t>(entries)) {
      checks.Expect(sample.packing[i] == T{-23});
    }
  }
  ++checks.profiles;
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (int exponent :
       {-40, -2, 0, 20, 40, std::numeric_limits<Real>::min_exponent - 1,
        std::numeric_limits<Real>::max_exponent - 10}) {
    for (asc::extent_t n : {0, 1, 2, 3, 5}) {
      for (auto triangle : {kUpper, kLower}) {
        for (auto layout : {kColumn, kRow}) {
          const Profile p{n, triangle, layout};
          Run<T>(checks, provider, p, exponent, -1);
          if (n != 0) {
            Run<T>(checks, provider, p, exponent, 0);
            if (n > 1) {
              Run<T>(checks, provider, p, exponent, n - 1);
            }
          }
        }
      }
    }
  }
}
}  // namespace

int main() {
  const asc_lapack_test::NormalReturnGuard normal_return;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  checks.Expect(checks.profiles == 1344);
  std::printf(
      "Packed Cholesky inverse public: %zu profiles, %zu checks, %zu "
      "failures\n",
      checks.profiles, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
