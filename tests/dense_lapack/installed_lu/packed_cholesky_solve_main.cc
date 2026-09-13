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
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_packed_solve.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Wide;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kPacking =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

struct Checks {
  std::size_t workflows = 0;
  std::size_t checks = 0;
  std::size_t failures = 0;
  void Expect(bool value) {
    ++checks;
    if (!value) {
      if (failures < 20) {
        std::fprintf(stderr, "Packed Cholesky solve check failed at %zu\n",
                     checks);
      }
      ++failures;
    }
  }
};

struct Profile {
  asc::extent_t n;
  asc::extent_t nrhs;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout a_layout;
  asc::DenseBlasLayout b_layout;
  bool complex_diagonal;
};

template <typename Visit>
void Slots(const Profile& p, const Visit& visit) {
  std::size_t slot = 0;
  for (asc::extent_t major = 0; major < p.n; ++major) {
    for (asc::extent_t minor = 0; minor < p.n; ++minor) {
      const auto i = p.a_layout == kColumn ? minor : major;
      const auto j = p.a_layout == kColumn ? major : minor;
      if (p.triangle == kUpper ? i <= j : i >= j) {
        visit(slot++, i, j);
      }
    }
  }
}

template <typename T>
Wide Lower(asc::extent_t i, asc::extent_t j, bool complex_diagonal) {
  if (i < j) {
    return {};
  }
  if (i == j) {
    return {static_cast<long double>(i + 2),
            complex_diagonal ? static_cast<long double>(i + 1) / 4 : 0};
  }
  const auto sign = (i + j) % 2 == 0 ? 1 : -1;
  return {sign * static_cast<long double>(i + 1) / 8,
          asc::DenseBlasComplex<T> ? static_cast<long double>(j + 1) / 16 : 0};
}

template <typename T>
Wide Matrix(const Profile& p, asc::extent_t i, asc::extent_t j) {
  Wide result{};
  for (asc::extent_t k = 0; k < p.n; ++k) {
    result += Lower<T>(i, k, p.complex_diagonal) *
              std::conj(Lower<T>(j, k, p.complex_diagonal));
  }
  return result;
}

template <typename T>
Wide Solution(asc::extent_t i, asc::extent_t j, int repeat) {
  return {static_cast<long double>(8 - 2 * i + j + repeat) / 8,
          asc::DenseBlasComplex<T>
              ? static_cast<long double>(i + 2 * j + repeat) / 16
              : 0};
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
  std::array<T, 64> rhs;
  std::array<T, 64> packing;
  explicit Fixture(const Profile& p, int exponent) {
    factors.fill(T{-91});
    rhs.fill(T{-93});
    packing.fill(T{-97});
    const auto scale = std::ldexp(1.0L, exponent / 2);
    Slots(p, [&](std::size_t slot, auto i, auto j) {
      const auto value = p.triangle == kUpper
                             ? std::conj(Lower<T>(j, i, p.complex_diagonal))
                             : Lower<T>(i, j, p.complex_diagonal);
      factors[slot + 1] = Narrow<T>(value * scale);
    });
  }
  static asc::stride_t Leading(const Profile& p) {
    return (p.b_layout == kRow ? p.nrhs : p.n) + 2;
  }
  static std::size_t Offset(const Profile& p, asc::extent_t i,
                            asc::extent_t j) {
    return static_cast<std::size_t>(p.b_layout == kRow ? i * Leading(p) + j
                                                       : j * Leading(p) + i) +
           1;
  }
  [[nodiscard]] auto A(const Profile& p) const {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        factors.data() + 1, p.n, p.a_layout,
        {factors.data(), sizeof(factors), kHost}));
  }
  auto B(const Profile& p) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        rhs.data() + 1, p.n, p.nrhs, p.b_layout, Leading(p),
        {rhs.data(), sizeof(rhs), kHost}));
  }
  void RightHandSide(const Profile& p, int exponent, int repeat) {
    const auto scale = std::ldexp(1.0L, exponent);
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        Wide value{};
        for (asc::extent_t k = 0; k < p.n; ++k) {
          value += Matrix<T>(p, i, k) * Solution<T>(k, j, repeat);
        }
        rhs[Offset(p, i, j)] = Narrow<T>(value * scale);
      }
    }
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace result;
    const auto entries = plan.regions[kPacking].minimum_entries;
    if (entries != 0) {
      result.regions[kPacking] = {packing.data() + 1,
                                  static_cast<std::size_t>(entries) * sizeof(T),
                                  kHost};
    }
    return result;
  }
};

template <typename T>
void Equation(Checks& checks, const Fixture<T>& sample, const Profile& p,
              const std::array<T, 64>& input, int exponent, int repeat) {
  using Real = asc::DenseBlasRealType<T>;
  const auto scale = std::ldexp(1.0L, exponent);
  const long double tolerance =
      128 * std::numeric_limits<Real>::epsilon() * p.n;
  long double norm_a = 0;
  for (asc::extent_t i = 0; i < p.n; ++i) {
    long double row_sum = 0;
    for (asc::extent_t k = 0; k < p.n; ++k) {
      row_sum += std::abs(Matrix<T>(p, i, k));
    }
    norm_a = std::max(norm_a, row_sum);
  }
  for (asc::extent_t j = 0; j < p.nrhs; ++j) {
    long double residual = 0;
    long double norm_x = 0;
    long double norm_b = 0;
    for (asc::extent_t i = 0; i < p.n; ++i) {
      const auto x =
          installed_internal::Widen(sample.rhs[Fixture<T>::Offset(p, i, j)]);
      checks.Expect(std::isfinite(x.real()) && std::isfinite(x.imag()));
      checks.Expect(std::abs(x - Solution<T>(i, j, repeat)) <= tolerance);
      const auto b =
          installed_internal::Widen(input[Fixture<T>::Offset(p, i, j)]) / scale;
      Wide ax{};
      for (asc::extent_t k = 0; k < p.n; ++k) {
        ax += Matrix<T>(p, i, k) * installed_internal::Widen(
                                       sample.rhs[Fixture<T>::Offset(p, k, j)]);
      }
      const auto error = std::abs(ax - b);
      checks.Expect(std::isfinite(error));
      residual = std::max(residual, error);
      norm_x = std::max(norm_x, std::abs(x));
      norm_b = std::max(norm_b, std::abs(b));
    }
    const auto denominator = norm_a * norm_x + norm_b;
    checks.Expect(denominator == 0 ? residual == 0
                                   : residual / denominator <= tolerance);
  }
}

template <typename T>
void Run(Checks& checks, const asc::ReferenceLapackProvider& provider,
         const Profile& p, int exponent) {
  Fixture<T> sample(p, exponent);
  const auto original_factors = sample.factors;
  const auto plan = Take(
      asc::QueryPptrsWorkspace(provider, p.triangle, sample.A(p), sample.B(p)));
  const bool active = p.n != 0 && p.nrhs != 0;
  const asc::extent_t expected_entries =
      !active ? 0
              : (p.a_layout == kRow ? p.n * (p.n + 1) / 2 : 0) +
                    (p.b_layout == kRow ? p.n * p.nrhs : 0);
  checks.Expect(plan.regions[kPacking].minimum_entries == expected_entries &&
                plan.regions[kPacking].preferred_entries == expected_entries);
  const auto workspace = sample.Workspace(plan);
  for (int repeat = 0; repeat < 2; ++repeat) {
    sample.RightHandSide(p, exponent, repeat);
    auto expected_backing = sample.rhs;
    const auto original_rhs = sample.rhs;
    asc::LapackReport report;
    const auto status = asc::Pptrs(provider, p.triangle, sample.A(p),
                                   sample.B(p), plan, workspace, report);
    checks.Expect(
        status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
        report.output_validity == asc::LapackOutputValidity::kComplete);
    checks.Expect(report.called_provider == active);
    checks.Expect(active ? report.native_info == 0 : !report.native_info);
    checks.Expect(sample.factors == original_factors);
    Equation(checks, sample, p, original_rhs, exponent, repeat);
    for (asc::extent_t i = 0; i < p.n; ++i) {
      for (asc::extent_t j = 0; j < p.nrhs; ++j) {
        expected_backing[Fixture<T>::Offset(p, i, j)] =
            sample.rhs[Fixture<T>::Offset(p, i, j)];
      }
    }
    checks.Expect(sample.rhs == expected_backing);
    checks.Expect(sample.packing.front() == T{-97});
    checks.Expect(std::all_of(sample.packing.begin() + 1 + expected_entries,
                              sample.packing.end(),
                              [](T value) { return value == T{-97}; }));
    ++checks.workflows;
  }
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const asc::extent_t n : {0, 1, 2, 3, 5}) {
    for (const asc::extent_t nrhs : {0, 1, 3}) {
      for (auto triangle : {kUpper, asc::DenseBlasTriangle::kLower}) {
        for (auto a_layout : {kColumn, kRow}) {
          for (auto b_layout : {kColumn, kRow}) {
            for (const bool complex_diagonal : {false, true}) {
              if (complex_diagonal && !asc::DenseBlasComplex<T>) {
                continue;
              }
              const Profile p{n,        nrhs,     triangle,
                              a_layout, b_layout, complex_diagonal};
              for (int exponent :
                   {-40, -2, 0, 20, 40,
                    std::numeric_limits<Real>::min_exponent - 1,
                    std::numeric_limits<Real>::max_exponent - 10}) {
                Run<T>(checks, provider, p, exponent);
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
  const asc_lapack_test::NormalReturnGuard normal_return;
  Checks checks;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  Scalar<float>(checks, provider);
  Scalar<double>(checks, provider);
  Scalar<std::complex<float>>(checks, provider);
  Scalar<std::complex<double>>(checks, provider);
  checks.Expect(checks.workflows == 10080);
  std::printf(
      "Packed Cholesky solve public: %zu workflows, %zu checks, %zu failures\n",
      checks.workflows, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
