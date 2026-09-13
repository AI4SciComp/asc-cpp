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
#include "asc/dense/providers/lapack_cholesky_packed.h"
#include "factorization_support.h"
#include "normal_return_guard.h"

namespace {
using installed_internal::Take;
using installed_internal::Wide;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
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
        std::fprintf(stderr, "Packed Cholesky public check failed at %zu\n",
                     checks);
      }
      ++failures;
    }
  }
};

// Enumerate physical packed slots directly; do not reuse adapter offsets.
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
Wide Lower(asc::extent_t i, asc::extent_t j) {
  if (i < j) {
    return {};
  }
  if (i == j) {
    return {static_cast<long double>(i + 2), 0};
  }
  const auto sign = (i + j) % 2 == 0 ? 1 : -1;
  return {sign * static_cast<long double>(i + 1) / 8,
          asc::DenseBlasComplex<T> ? static_cast<long double>(j + 1) / 16 : 0};
}

template <typename T>
Wide Matrix(asc::extent_t i, asc::extent_t j, asc::extent_t n) {
  Wide sum{};
  for (asc::extent_t k = 0; k < n; ++k) {
    sum += Lower<T>(i, k) * std::conj(Lower<T>(j, k));
  }
  return sum;
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
  std::array<T, 32> data;
  std::array<T, 32> scratch;
  Fixture() {
    data.fill(T{-91});
    scratch.fill(T{-97});
  }
  auto View(asc::extent_t n, asc::DenseBlasLayout layout) {
    return Take(asc::DenseBlasPackedMatrixView<T>::Create(
        data.data() + 1, n, layout, {data.data(), sizeof(data), kHost}));
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace work;
    const auto& packing = plan.regions[kPacking];
    if (packing.minimum_entries != 0) {
      work.regions[kPacking] = {
          scratch.data() + 1,
          static_cast<std::size_t>(packing.minimum_entries) * sizeof(T), kHost};
    }
    return work;
  }
  void Guards(Checks& checks, asc::extent_t n,
              const asc::LapackWorkspacePlan& plan) const {
    const auto count = static_cast<std::size_t>(n * (n + 1) / 2);
    const auto used =
        static_cast<std::size_t>(plan.regions[kPacking].minimum_entries);
    checks.Expect(data.front() == T{-91} && scratch.front() == T{-97});
    checks.Expect(std::all_of(data.begin() + 1 + count, data.end(),
                              [](T value) { return value == T{-91}; }));
    checks.Expect(std::all_of(scratch.begin() + 1 + used, scratch.end(),
                              [](T value) { return value == T{-97}; }));
  }
};

// Compare the complete matrix equation using normalized wide factors. The
// max-entry norm avoids overflow at both scalar exponent endpoints.
template <typename T>
void Reconstruction(Checks& checks, const Fixture<T>& sample, asc::extent_t n,
                    asc::DenseBlasTriangle triangle,
                    asc::DenseBlasLayout layout, long double factor_scale) {
  std::array<std::array<Wide, 5>, 5> factor{};
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    factor[i][j] =
        installed_internal::Widen(sample.data[slot + 1]) / factor_scale;
  });
  long double residual = 0;
  long double norm = 0;
  for (asc::extent_t i = 0; i < n; ++i) {
    checks.Expect(factor[i][i].real() > 0 && factor[i][i].imag() == 0);
    for (asc::extent_t j = 0; j < n; ++j) {
      Wide actual{};
      for (asc::extent_t k = 0; k < n; ++k) {
        actual += triangle == kUpper ? std::conj(factor[k][i]) * factor[k][j]
                                     : factor[i][k] * std::conj(factor[j][k]);
      }
      const auto expected = Matrix<T>(i, j, n);
      const auto difference = std::abs(actual - expected);
      checks.Expect(std::isfinite(difference));
      residual = std::max(residual, difference);
      norm = std::max(norm, std::abs(expected));
    }
  }
  // Small, well-conditioned dyadic fixtures: a conservative linear-in-N
  // accumulation bound, in the tested scalar's epsilon.
  const long double tolerance =
      128 * std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() * n;
  checks.Expect(norm == 0 ? residual == 0 : residual / norm <= tolerance);
}

template <typename T>
void Positive(Checks& checks, const asc::ReferenceLapackProvider& provider,
              asc::extent_t n, asc::DenseBlasTriangle triangle,
              asc::DenseBlasLayout layout, int exponent) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> sample;
  const long double scale = std::ldexp(1.0L, exponent);
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    auto value = Matrix<T>(i, j, n) * scale;
    if constexpr (asc::DenseBlasComplex<T>) {
      if (i == j) {
        value.imag(std::numeric_limits<long double>::quiet_NaN());
      }
    }
    sample.data[slot + 1] = Narrow<T>(value);
  });
  const auto a = sample.View(n, layout);
  const auto plan = Take(asc::QueryPptrfWorkspace(provider, triangle, a));
  auto work = sample.Workspace(plan);
  asc::LapackReport report;
  const auto status = asc::Pptrf(provider, triangle, a, plan, work, report);
  checks.Expect(status.ok() && report.outcome == asc::LapackOutcome::kSuccess &&
                report.output_validity == asc::LapackOutputValidity::kComplete);
  checks.Expect(report.called_provider == (n != 0));
  checks.Expect(n == 0 ? !report.native_info : report.native_info == 0);
  const long double factor_scale = std::ldexp(1.0L, exponent / 2);
  const long double tolerance = 128 * std::numeric_limits<Real>::epsilon() * n;
  // Unique positive-diagonal Cholesky factors form an independent oracle.
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    const Wide actual =
        installed_internal::Widen(sample.data[slot + 1]) / factor_scale;
    const Wide expected =
        triangle == kUpper ? std::conj(Lower<T>(j, i)) : Lower<T>(i, j);
    checks.Expect(std::isfinite(actual.real()) && std::isfinite(actual.imag()));
    checks.Expect(std::abs(actual - expected) <= tolerance * (n + 2));
  });
  Reconstruction(checks, sample, n, triangle, layout, factor_scale);
  sample.Guards(checks, n, plan);
  ++checks.workflows;
}

template <typename T>
void Nonpositive(Checks& checks, const asc::ReferenceLapackProvider& provider,
                 asc::extent_t n, asc::DenseBlasTriangle triangle,
                 asc::DenseBlasLayout layout, asc::extent_t pivot) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> sample;
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    if (i == j) {
      const auto real =
          i == pivot ? Real{-1} : static_cast<Real>((i + 2) * (i + 2));
      sample.data[slot + 1] = Narrow<T>(
          {real,
           asc::DenseBlasComplex<T> ? static_cast<long double>(17 + i) : 0});
    } else {
      sample.data[slot + 1] = T{};
    }
  });
  auto expected = sample.data;
  Slots(n, triangle, layout, [&](std::size_t slot, auto i, auto j) {
    if (i == j) {
      if (i < pivot) {
        expected[slot + 1] = T{static_cast<Real>(i + 2)};
      } else if (i == pivot) {
        expected[slot + 1] = T{-1};
      } else if (triangle != kUpper && pivot > 0) {
        expected[slot + 1] = T{static_cast<Real>((i + 2) * (i + 2))};
      }
    }
  });
  const auto a = sample.View(n, layout);
  const auto plan = Take(asc::QueryPptrfWorkspace(provider, triangle, a));
  auto work = sample.Workspace(plan);
  asc::LapackReport report;
  const auto status = asc::Pptrf(provider, triangle, a, plan, work, report);
  checks.Expect(status.code() == asc::ErrorCode::kNumerical);
  checks.Expect(report.native_info == pivot + 1 &&
                report.diagnostic_index == pivot);
  checks.Expect(report.called_provider &&
                report.outcome == asc::LapackOutcome::kNotPositiveDefinite &&
                report.output_validity ==
                    asc::LapackOutputValidity::kDocumentedPartial);
  checks.Expect(sample.data == expected);
  sample.Guards(checks, n, plan);
  ++checks.workflows;
}

template <typename T>
void Scalar(Checks& checks, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const asc::extent_t n : {0, 1, 2, 3, 5}) {
    for (auto triangle : {kUpper, asc::DenseBlasTriangle::kLower}) {
      for (auto layout : {kColumn, asc::DenseBlasLayout::kRowMajor}) {
        for (int exponent :
             {-40, -2, 0, 20, 40, std::numeric_limits<Real>::min_exponent - 1,
              std::numeric_limits<Real>::max_exponent - 10}) {
          Positive<T>(checks, provider, n, triangle, layout, exponent);
        }
        if (n > 0) {
          for (const auto pivot : {asc::extent_t{0}, n - 1}) {
            Nonpositive<T>(checks, provider, n, triangle, layout, pivot);
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
  checks.Expect(checks.workflows == 688);
  std::printf(
      "Packed Cholesky public: %zu workflows, %zu checks, %zu failures\n",
      checks.workflows, checks.checks, checks.failures);
  return checks.failures == 0 ? 0 : 1;
}
