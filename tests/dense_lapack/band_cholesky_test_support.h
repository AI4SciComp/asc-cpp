#ifndef ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "../allocation_observation.h"
#include "../dense/allocation_probe.h"
#include "../dense/test_support.h"
#include "allocation_audit.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"

namespace asc_band_test {
using asc_dense_test::TestContext;
using Wide = std::complex<long double>;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kUpper = asc::DenseBlasTriangle::kUpper;
constexpr auto kLower = asc::DenseBlasTriangle::kLower;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Take(asc::Result<T> result) {
  if (!result.ok()) {
    std::fprintf(stderr, "Unexpected band setup failure %d\n",
                 static_cast<int>(result.status().code()));
    std::abort();
  }
  return std::move(*result);
}
template <typename Function>
auto WithoutAllocation(TestContext& test, Function function) {
  asc_dense_test::AllocationProbe cpp_probe;
  asc_lapack_test::BeginAllocationAudit();
  auto result = function();
  const auto c_calls = asc_lapack_test::EndAllocationAudit();
  const auto cpp_calls = cpp_probe.count();
  ASC_DENSE_TEST_EQ(test, c_calls, 0U);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(cpp_calls, 0));
  return result;
}
template <typename T>
T Value(long double real, long double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return {static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}
template <typename T>
Wide ToWide(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return {value.real(), value.imag()};
  } else {
    return {value, 0};
  }
}
template <typename T>
bool SameBytes(const std::vector<T>& a, const std::vector<T>& b) {
  return a.size() == b.size() &&
         std::memcmp(a.data(), b.data(), a.size() * sizeof(T)) == 0;
}
// Exact representations intentionally distinguish signed zero and retain NaN
// payloads; this is a publication/fidelity oracle, not numerical equality.
template <typename T>
bool SameScalarBytes(const T& a, const T& b) {
  using Bytes = std::array<std::byte, sizeof(T)>;
  return std::bit_cast<Bytes>(a) == std::bit_cast<Bytes>(b);
}

template <typename T>
struct BandData {
  asc::extent_t n;
  asc::extent_t kd;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  asc::extent_t ld;
  std::vector<T> values;
  std::vector<Wide> original;
  std::vector<Wide> expected;
  std::vector<unsigned char> selected;

  BandData(asc::extent_t order, asc::extent_t bandwidth,
           asc::DenseBlasTriangle part, asc::DenseBlasLayout storage_layout,
           int scale_exponent = 0)
      : n(order),
        kd(bandwidth),
        triangle(part),
        layout(storage_layout),
        ld(kd + 3),
        values(static_cast<std::size_t>(n * ld + 2), Value<T>(-79, 31)),
        original(static_cast<std::size_t>(n * n)),
        expected(static_cast<std::size_t>(n * n)),
        selected(values.size()) {
    Build(scale_exponent);
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    if (layout == kColumn) {
      return static_cast<std::size_t>(
          1 + j * ld + (triangle == kUpper ? kd + i - j : i - j));
    }
    return static_cast<std::size_t>(1 + i * ld +
                                    (triangle == kUpper ? j - i : kd + j - i));
  }
  [[nodiscard]] bool Selected(asc::extent_t i, asc::extent_t j) const {
    return triangle == kUpper ? i <= j && j - i <= kd : j <= i && i - j <= kd;
  }
  asc::LapackPositiveDefiniteBandView<T> View(asc::MemorySpace space = kHost) {
    return Take(asc::LapackPositiveDefiniteBandView<T>::Create(
        values.data() + 1, n, kd, triangle, layout, ld,
        {values.data(), values.size() * sizeof(T), space}));
  }
  [[nodiscard]] asc::LapackPositiveDefiniteBandView<const T> ConstView(
      asc::MemorySpace space = kHost) const {
    return Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
        values.data() + 1, n, kd, triangle, layout, ld,
        {values.data(), values.size() * sizeof(T), space}));
  }
  void Build(int exponent) {
    std::vector<Wide> lower(static_cast<std::size_t>(n * n));
    const long double scale = std::ldexp(1.0L, exponent);
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j <= i; ++j) {
        T coefficient{};
        if (i == j) {
          coefficient = Value<T>(2);
        } else if (i - j <= kd) {
          if (i - j == 1) {
            coefficient = Value<T>(0.125L, 0.0625L);
          } else if (i - j == 2) {
            coefficient = Value<T>(-0.0625L, 0.03125L);
          } else if (i - j == kd) {
            coefficient = Value<T>(0.125L, -0.03125L);
          }
        }
        lower[static_cast<std::size_t>(i * n + j)] =
            ToWide(coefficient) * scale;
      }
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j < n; ++j) {
        Wide a{};
        for (asc::extent_t k = 0; k <= std::min(i, j); ++k) {
          a += lower[static_cast<std::size_t>(i * n + k)] *
               std::conj(lower[static_cast<std::size_t>(j * n + k)]);
        }
        original[static_cast<std::size_t>(i * n + j)] = a;
        expected[static_cast<std::size_t>(i * n + j)] =
            triangle == kLower
                ? lower[static_cast<std::size_t>(i * n + j)]
                : std::conj(lower[static_cast<std::size_t>(j * n + i)]);
        if (Selected(i, j)) {
          values[Index(i, j)] = Value<T>(a.real(), i == j ? 53 : a.imag());
          selected[Index(i, j)] = 1;
        }
      }
    }
  }
  void CheckPadding(TestContext& test, const std::vector<T>& before) const {
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (selected[i] == 0) {
        ASC_DENSE_TEST_CHECK(test, SameScalarBytes(values[i], before[i]));
      }
    }
  }
  [[nodiscard]] Wide Factor(asc::extent_t i, asc::extent_t j) const {
    return Selected(i, j) ? ToWide(values[Index(i, j)]) : Wide{};
  }
  void CheckFactor(TestContext& test) const {
    long double error = 0;
    long double norm = 0;
    const auto eps = std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    for (asc::extent_t i = 0; i < n; ++i) {
      long double error_row = 0;
      long double original_row = 0;
      for (asc::extent_t j = 0; j < n; ++j) {
        Wide reconstructed{};
        for (asc::extent_t k = 0; k < n; ++k) {
          reconstructed += triangle == kLower
                               ? Factor(i, k) * std::conj(Factor(j, k))
                               : std::conj(Factor(k, i)) * Factor(k, j);
        }
        const auto a = original[static_cast<std::size_t>(i * n + j)];
        error_row += std::abs(reconstructed - a);
        original_row += std::abs(a);
      }
      error = std::max(error, error_row);
      norm = std::max(norm, original_row);
      const auto diagonal = Factor(i, i);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(diagonal.real()) &&
                                     diagonal.real() > 0 &&
                                     diagonal.imag() == 0);
    }
    ASC_DENSE_TEST_CHECK(
        test, norm == 0 ? error == 0
                        : std::isfinite(error) &&
                              error / norm <=
                                  128 * std::max<asc::extent_t>(1, n) * eps);
  }
};

template <typename T>
struct RhsData {
  asc::extent_t n;
  asc::extent_t count;
  asc::DenseBlasLayout layout;
  asc::extent_t ld;
  std::vector<T> values;
  std::vector<Wide> original;
  std::vector<Wide> expected;
  RhsData(const BandData<T>& band, asc::extent_t nrhs,
          asc::DenseBlasLayout order)
      : n(band.n),
        count(nrhs),
        layout(order),
        ld((order == kColumn ? n : count) + 2),
        values(
            static_cast<std::size_t>((order == kColumn ? count : n) * ld + 2),
            Value<T>(-83, 17)),
        original(static_cast<std::size_t>(n * count)),
        expected(static_cast<std::size_t>(n * count)) {
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j < count; ++j) {
        expected[static_cast<std::size_t>(i * count + j)] =
            ToWide(Value<T>((i % 3) - j + 1, (i + j) % 2 != 0 ? 0.25L : -0.5L));
      }
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j < count; ++j) {
        Wide b{};
        for (asc::extent_t k = 0; k < n; ++k) {
          b += band.original[static_cast<std::size_t>(i * n + k)] *
               expected[static_cast<std::size_t>(k * count + j)];
        }
        values[Index(i, j)] = Value<T>(b.real(), b.imag());
        original[static_cast<std::size_t>(i * count + j)] =
            ToWide(values[Index(i, j)]);
      }
    }
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * ld + i : i * ld + j));
  }
  asc::DenseBlasMatrixView<T> View(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 1, n, count, layout, ld,
        {values.data(), values.size() * sizeof(T), space}));
  }
  void Check(TestContext& test, const BandData<T>& band,
             const std::vector<T>& before) const {
    std::vector<unsigned char> selected(values.size());
    long double error = 0;
    long double a_norm = 0;
    long double x_norm = 0;
    long double b_norm = 0;
    const auto eps = std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    for (asc::extent_t i = 0; i < n; ++i) {
      long double a_row = 0;
      for (asc::extent_t k = 0; k < n; ++k) {
        a_row += std::abs(band.original[static_cast<std::size_t>(i * n + k)]);
      }
      a_norm = std::max(a_norm, a_row);
      for (asc::extent_t j = 0; j < count; ++j) {
        selected[Index(i, j)] = 1;
        const Wide x = ToWide(values[Index(i, j)]);
        const Wide goal = expected[static_cast<std::size_t>(i * count + j)];
        ASC_DENSE_TEST_CHECK(test, std::isfinite(std::abs(x)));
        ASC_DENSE_TEST_CHECK(
            test, std::abs(x - goal) <= 128 * std::max<asc::extent_t>(1, n) *
                                            eps *
                                            std::max(1.0L, std::abs(goal)));
        x_norm = std::max(x_norm, std::abs(x));
        Wide product{};
        for (asc::extent_t k = 0; k < n; ++k) {
          product += band.original[static_cast<std::size_t>(i * n + k)] *
                     ToWide(values[Index(k, j)]);
        }
        const auto b = original[static_cast<std::size_t>(i * count + j)];
        error = std::max(error, std::abs(product - b));
        b_norm = std::max(b_norm, std::abs(b));
      }
    }
    const auto denominator = a_norm * x_norm + b_norm;
    ASC_DENSE_TEST_CHECK(
        test, denominator == 0
                  ? error == 0
                  : std::isfinite(error) &&
                        error / denominator <=
                            128 * std::max<asc::extent_t>(1, n) * eps);
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (selected[i] == 0) {
        ASC_DENSE_TEST_CHECK(test, SameScalarBytes(values[i], before[i]));
      }
    }
  }
};
template <typename T>
struct Storage {
  std::vector<T> values;
  explicit Storage(const asc::LapackWorkspacePlan& plan)
      : values(
            static_cast<std::size_t>(plan.regions[kLayout].preferred_entries) +
                2,
            Value<T>(-97, 43)) {}
  asc::LapackWorkspace View() {
    asc::LapackWorkspace result;
    result.regions[kLayout] = {values.data() + 1,
                               (values.size() - 2) * sizeof(T), kHost};
    return result;
  }
  void Check(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, values.front(), Value<T>(-97, 43));
    ASC_DENSE_TEST_EQ(test, values.back(), Value<T>(-97, 43));
  }
};
}  // namespace asc_band_test

#endif  // ASC_TESTS_DENSE_LAPACK_BAND_CHOLESKY_TEST_SUPPORT_H_
