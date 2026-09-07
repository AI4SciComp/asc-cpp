#ifndef ASC_TESTS_DENSE_LAPACK_LU_BAND_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_BAND_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "band_cholesky_test_support.h"

namespace asc_lu_band_test {
using asc_band_test::SameBytes;
using asc_band_test::SameScalarBytes;
using asc_band_test::Take;
using asc_band_test::ToWide;
using asc_band_test::Value;
using asc_band_test::Wide;
using asc_band_test::WithoutAllocation;
using asc_dense_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr std::array kOperations{asc::DenseBlasTranspose::kNone,
                                 asc::DenseBlasTranspose::kTranspose,
                                 asc::DenseBlasTranspose::kConjugateTranspose};
constexpr auto kInteger =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

// Integer storage is public byte storage with enough native alignment; the
// wrapper starts its own native lifetimes. Layout storage contains live T.
// Both buffers have exact-size exposed regions and adjacent red zones.
template <typename T>
struct Scratch {
  std::vector<T> layout;
  std::vector<std::max_align_t> integer;
  std::size_t integer_bytes;
  explicit Scratch(const asc::LapackWorkspacePlan& plan)
      : layout(
            static_cast<std::size_t>(plan.regions[kLayout].preferred_entries) +
                2,
            Value<T>(-131, 29)),
        integer((static_cast<std::size_t>(
                     plan.regions[kInteger].preferred_entries) *
                     plan.regions[kInteger].entry_bytes +
                 sizeof(std::max_align_t) - 1) /
                    sizeof(std::max_align_t) +
                2),
        integer_bytes(
            static_cast<std::size_t>(plan.regions[kInteger].preferred_entries) *
            plan.regions[kInteger].entry_bytes) {
    std::fill_n(reinterpret_cast<std::byte*>(integer.data()),
                integer.size() * sizeof(std::max_align_t), std::byte{0x6b});
  }
  asc::LapackWorkspace View() {
    asc::LapackWorkspace result;
    result.regions[kLayout] = {layout.data() + 1,
                               (layout.size() - 2) * sizeof(T), kHost};
    result.regions[kInteger] = {integer.data() + 1, integer_bytes, kHost};
    return result;
  }
  void Check(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, layout.front(), Value<T>(-131, 29));
    ASC_DENSE_TEST_EQ(test, layout.back(), Value<T>(-131, 29));
    const auto* bytes = reinterpret_cast<const std::byte*>(integer.data());
    for (std::size_t i = 0; i < integer.size() * sizeof(std::max_align_t);
         ++i) {
      if (i < sizeof(std::max_align_t) ||
          i >= sizeof(std::max_align_t) + integer_bytes) {
        ASC_DENSE_TEST_EQ(test, bytes[i], std::byte{0x6b});
      }
    }
  }
};

struct Pivots {
  std::vector<asc::index_t> values;
  explicit Pivots(asc::extent_t count)
      : values(static_cast<std::size_t>(count) + 2, -179) {}
  auto View(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
        values.data() + 1, static_cast<asc::extent_t>(values.size() - 2), 1,
        {values.data(), values.size() * sizeof(asc::index_t), space}));
  }
  [[nodiscard]] auto ConstView(asc::MemorySpace space = kHost) const {
    return Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
        values.data() + 1, static_cast<asc::extent_t>(values.size() - 2), 1,
        {values.data(), values.size() * sizeof(asc::index_t), space}));
  }
  void Check(TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, values.front(), -179);
    ASC_DENSE_TEST_EQ(test, values.back(), -179);
  }
};

template <typename T>
struct Band {
  asc::extent_t m;
  asc::extent_t n;
  asc::extent_t kl;
  asc::extent_t ku;
  asc::extent_t ld;
  std::vector<T> values;
  std::vector<Wide> original;
  Band(asc::extent_t rows, asc::extent_t columns, asc::extent_t lower,
       asc::extent_t upper, int exponent = 0)
      : m(rows),
        n(columns),
        kl(lower),
        ku(upper),
        ld(2 * kl + ku + 4),
        values(static_cast<std::size_t>(ld * n + 2), Value<T>(-137, 41)),
        original(static_cast<std::size_t>(m * n)) {
    const long double scale = std::ldexp(1.0L, exponent);
    const auto nan =
        std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
    // Every reserved/unused stored slot starts poisoned, distinct from ld
    // padding. Numerical reconstruction catches accidental coefficient reads.
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t row = 0; row < 2 * kl + ku + 1; ++row) {
        values[static_cast<std::size_t>(1 + j * ld + row)] = Value<T>(nan, nan);
      }
      for (asc::extent_t i = std::max<asc::extent_t>(0, j - ku);
           i < std::min(m, j + kl + 1); ++i) {
        Put(i, j,
            Value<T>((i == j ? 8.0L : ((i + 3 * j) % 5 - 2) / 128.0L) * scale,
                     i == j ? 0 : ((2 * i + j) % 3 - 1) * scale / 256.0L));
      }
    }
    // Far band pivots force the blocked WORK31 path when KL>=32 and KU>64.
    if (kl > 0 && ku >= kl && std::min(m, n) > kl) {
      for (const asc::extent_t first : {asc::extent_t{0}, asc::extent_t{33}}) {
        const auto last = first + kl;
        if (last < std::min(m, n) && (first == 0 || first > kl)) {
          Put(first, first, T{});
          Put(last, last, T{});
          Put(first, last, Value<T>(4 * scale, scale / 2));
          Put(last, first, Value<T>(8 * scale, -scale / 4));
        }
      }
    }
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(1 + j * ld + kl + ku + i - j);
  }
  void Put(asc::extent_t i, asc::extent_t j, T value) {
    values[Index(i, j)] = value;
    original[static_cast<std::size_t>(i * n + j)] = ToWide(value);
  }
  auto View(asc::MemorySpace space = kHost) {
    return Take(asc::LapackLuBandView<T>::Create(
        values.data() + 1, m, n, kl, ku, ld,
        {values.data(), values.size() * sizeof(T), space}));
  }
  [[nodiscard]] auto ConstView(asc::MemorySpace space = kHost) const {
    return Take(asc::LapackLuBandView<const T>::Create(
        values.data() + 1, m, n, kl, ku, ld,
        {values.data(), values.size() * sizeof(T), space}));
  }
  void Padding(TestContext& test, const std::vector<T>& before) const {
    ASC_DENSE_TEST_CHECK(test, SameScalarBytes(values.front(), before.front()));
    ASC_DENSE_TEST_CHECK(test, SameScalarBytes(values.back(), before.back()));
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t row = 2 * kl + ku + 1; row < ld; ++row) {
        const auto i = static_cast<std::size_t>(1 + j * ld + row);
        ASC_DENSE_TEST_CHECK(test, SameScalarBytes(values[i], before[i]));
      }
    }
  }
  void Reconstruct(TestContext& test, const Pivots& pivots) const {
    std::vector<Wide> reconstructed(static_cast<std::size_t>(m * n));
    for (asc::extent_t i = 0; i < std::min(m, n); ++i) {
      for (asc::extent_t j = i; j < std::min(n, i + kl + ku + 1); ++j) {
        reconstructed[static_cast<std::size_t>(i * n + j)] =
            ToWide(values[Index(i, j)]);
      }
    }
    // Reverse each elimination then its swap, in reverse step order. This is
    // independent full-matrix arithmetic and honors interleaved band pivots;
    // a dense GETRF P*L*U interpretation would be wrong for these factors.
    for (asc::extent_t j = std::min(m, n); j-- > 0;) {
      for (asc::extent_t i = j + 1; i < std::min(m, j + kl + 1); ++i) {
        const Wide multiplier = ToWide(values[Index(i, j)]);
        for (asc::extent_t k = 0; k < n; ++k) {
          reconstructed[static_cast<std::size_t>(i * n + k)] +=
              multiplier * reconstructed[static_cast<std::size_t>(j * n + k)];
        }
      }
      const auto p = pivots.values[static_cast<std::size_t>(j + 1)] - 1;
      ASC_DENSE_TEST_CHECK(test, p >= j && p < std::min(m, j + kl + 1));
      if (p < j || p >= std::min(m, j + kl + 1)) {
        return;
      }
      for (asc::extent_t k = 0; k < n; ++k) {
        std::swap(reconstructed[static_cast<std::size_t>(j * n + k)],
                  reconstructed[static_cast<std::size_t>(p * n + k)]);
      }
    }
    long double error = 0;
    long double norm = 0;
    for (asc::extent_t i = 0; i < m; ++i) {
      long double row_error = 0;
      long double row_norm = 0;
      for (asc::extent_t j = 0; j < n; ++j) {
        const auto at = static_cast<std::size_t>(i * n + j);
        row_error += std::abs(reconstructed[at] - original[at]);
        row_norm += std::abs(original[at]);
      }
      // A NaN cannot be hidden by std::max's comparison ordering.
      ASC_DENSE_TEST_CHECK(test,
                           std::isfinite(row_error) && std::isfinite(row_norm));
      error = std::max(error, row_error);
      norm = std::max(norm, row_norm);
    }
    const long double tolerance =
        64 * std::max<asc::extent_t>(1, std::min(m, n)) *
        std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    ASC_DENSE_TEST_CHECK(test,
                         norm == 0 ? error == 0 : error / norm <= tolerance);
  }
};

template <typename T>
struct Rhs {
  asc::extent_t n;
  asc::extent_t count;
  asc::DenseBlasLayout layout;
  asc::extent_t ld;
  asc::DenseBlasTranspose operation;
  std::vector<T> values;
  std::vector<Wide> original;
  std::vector<Wide> expected;
  Rhs(const Band<T>& band, asc::extent_t nrhs, asc::DenseBlasLayout order,
      asc::DenseBlasTranspose op)
      : n(band.n),
        count(nrhs),
        layout(order),
        ld((layout == kColumn ? n : count) + 3),
        operation(op),
        values(
            static_cast<std::size_t>((layout == kColumn ? count : n) * ld + 2),
            Value<T>(-139, 43)),
        original(static_cast<std::size_t>(n * count)),
        expected(static_cast<std::size_t>(n * count)) {
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j < count; ++j) {
        expected[static_cast<std::size_t>(i * count + j)] =
            ToWide(Value<T>((i % 3 - 1) + j / 4.0L, ((i + j) % 3 - 1) / 8.0L));
      }
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      for (asc::extent_t j = 0; j < count; ++j) {
        Wide b{};
        for (asc::extent_t k = 0; k < n; ++k) {
          b += Coefficient(band, i, k) *
               expected[static_cast<std::size_t>(k * count + j)];
        }
        values[Index(i, j)] = Value<T>(b.real(), b.imag());
        original[static_cast<std::size_t>(i * count + j)] =
            ToWide(values[Index(i, j)]);
      }
    }
  }
  [[nodiscard]] Wide Coefficient(const Band<T>& band, asc::extent_t i,
                                 asc::extent_t j) const {
    const auto a = band.original[static_cast<std::size_t>(
        operation == asc::DenseBlasTranspose::kNone ? i * n + j : j * n + i)];
    return operation == asc::DenseBlasTranspose::kConjugateTranspose
               ? std::conj(a)
               : a;
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * ld + i : i * ld + j));
  }
  auto View(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        values.data() + 1, n, count, layout, ld,
        {values.data(), values.size() * sizeof(T), space}));
  }
  void Check(TestContext& test, const Band<T>& band,
             const std::vector<T>& before) const {
    std::vector<unsigned char> selected(values.size());
    long double error = 0;
    long double a_norm = 0;
    long double x_norm = 0;
    long double b_norm = 0;
    long double forward = 0;
    long double expected_norm = 0;
    for (asc::extent_t i = 0; i < n; ++i) {
      long double a_row = 0;
      for (asc::extent_t k = 0; k < n; ++k) {
        a_row += std::abs(Coefficient(band, i, k));
      }
      a_norm = std::max(a_norm, a_row);
      long double e_row = 0;
      long double x_row = 0;
      long double b_row = 0;
      long double f_row = 0;
      long double expected_row = 0;
      for (asc::extent_t j = 0; j < count; ++j) {
        selected[Index(i, j)] = 1;
        const auto at = static_cast<std::size_t>(i * count + j);
        const Wide x = ToWide(values[Index(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(x.real()) && std::isfinite(x.imag()));
        Wide residual = -original[at];
        for (asc::extent_t k = 0; k < n; ++k) {
          residual += Coefficient(band, i, k) * ToWide(values[Index(k, j)]);
        }
        e_row += std::abs(residual);
        x_row += std::abs(x);
        b_row += std::abs(original[at]);
        f_row += std::abs(x - expected[at]);
        expected_row += std::abs(expected[at]);
      }
      ASC_DENSE_TEST_CHECK(test, std::isfinite(e_row) && std::isfinite(f_row));
      error = std::max(error, e_row);
      x_norm = std::max(x_norm, x_row);
      b_norm = std::max(b_norm, b_row);
      forward = std::max(forward, f_row);
      expected_norm = std::max(expected_norm, expected_row);
    }
    const long double denominator = a_norm * x_norm + b_norm;
    const long double tolerance =
        64 * std::max<asc::extent_t>(1, n) *
        std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    ASC_DENSE_TEST_CHECK(
        test, denominator == 0 ? error == 0 : error / denominator <= tolerance);
    ASC_DENSE_TEST_CHECK(test, expected_norm == 0
                                   ? forward == 0
                                   : forward / expected_norm <= tolerance * 16);
    for (std::size_t i = 0; i < values.size(); ++i) {
      if (selected[i] == 0) {
        ASC_DENSE_TEST_CHECK(test, SameScalarBytes(values[i], before[i]));
      }
    }
  }
};

}  // namespace asc_lu_band_test

#endif  // ASC_TESTS_DENSE_LAPACK_LU_BAND_TEST_SUPPORT_H_
