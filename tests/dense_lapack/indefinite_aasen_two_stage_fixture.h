#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_FIXTURE_H_

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "asc/dense/blas.h"
#include "indefinite_rook_test_support.h"

namespace asc_aasen_two_stage_test {
namespace base = asc_indefinite_rook_test;

// The band LU has interleaved row swaps. Reverse elimination first, then
// recover the shifted unit triangular factor and undo the symmetric pivots.
template <typename T, typename Integer>
void Reconstruction(base::TestContext& test, int n, bool he, bool upper,
                    const T* a, int lda, const T* tb, int ldtb, int nb,
                    const Integer* pivots, const Integer* band_pivots,
                    std::span<const base::Wide> original) {
  using base::Wide;
  std::vector<Wide> band(static_cast<std::size_t>(n * n));
  std::vector<Wide> factor(band.size());
  std::vector<Wide> intermediate(band.size());
  std::vector<Wide> product(band.size());
  for (int i = 0; i < n; ++i) {
    for (int j = i; j < std::min(n, i + 2 * nb + 1); ++j) {
      band[i * n + j] = base::ToWide(tb[j * ldtb + 2 * nb + i - j]);
    }
    factor[i * n + i] = Wide{1, 0};
    for (int j = nb; j < i; ++j) {
      factor[i * n + j] =
          upper ? base::Adjoint(base::ToWide(a[i * lda + j - nb]), he)
                : base::ToWide(a[(j - nb) * lda + i]);
    }
  }
  for (int j = n - 1; j >= 0; --j) {
    for (int i = j + 1; i < std::min(n, j + nb + 1); ++i) {
      const auto multiplier = base::ToWide(tb[j * ldtb + 2 * nb + i - j]);
      for (int k = 0; k < n; ++k) {
        band[i * n + k] += multiplier * band[j * n + k];
      }
    }
    const auto p = band_pivots[j] - 1;
    ASC_DENSE_TEST_CHECK(test, p >= j && p < std::min(n, j + nb + 1));
    if (p < j || p >= std::min(n, j + nb + 1)) {
      return;
    }
    for (int k = 0; k < n; ++k) {
      std::swap(band[j * n + k], band[p * n + k]);
    }
  }
  for (int i = 0; i < n; ++i) {
    for (int k = 0; k <= i; ++k) {
      for (int j = 0; j < n; ++j) {
        intermediate[i * n + j] += factor[i * n + k] * band[k * n + j];
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k <= j; ++k) {
        product[i * n + j] +=
            intermediate[i * n + k] * base::Adjoint(factor[j * n + k], he);
      }
    }
  }
  for (int i = n - 1; i >= 0; --i) {
    const auto p = pivots[i] - 1;
    ASC_DENSE_TEST_CHECK(test, p >= i && p < n);
    ASC_DENSE_TEST_CHECK(test, i >= nb || p == i);
    if (p < i || p >= n) {
      return;
    }
    for (int j = 0; j < n; ++j) {
      std::swap(product[i * n + j], product[p * n + j]);
    }
    for (int j = 0; j < n; ++j) {
      std::swap(product[j * n + i], product[j * n + p]);
    }
  }
  long double error = 0;
  long double norm = 0;
  for (int i = 0; i < n * n; ++i) {
    ASC_DENSE_TEST_CHECK(test, std::isfinite(product[i].real()) &&
                                   std::isfinite(product[i].imag()));
    error = std::max(error, std::abs(product[i] - original[i]));
    norm = std::max(norm, std::abs(original[i]));
  }
  const long double tolerance =
      64 * std::max(n, 1) *
      std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon() * norm;
  if (!(error <= tolerance)) {
    std::fprintf(stderr,
                 "Two-stage reconstruction n=%d nb=%d he=%d upper=%d "
                 "error=%Lg limit=%Lg\n",
                 n, nb, he, upper, error, tolerance);
  }
  ASC_DENSE_TEST_CHECK(test, error <= tolerance);
}

template <typename T>
struct Sample {
  int n;
  int lda;
  bool hermitian;
  bool upper;
  std::vector<T> a;
  std::vector<T> before;
  std::vector<base::Wide> full;

  Sample(int order, bool he, bool use_upper, bool singular = false)
      : n(order),
        lda(std::max(1, order + 3)),
        hermitian(he),
        upper(use_upper),
        a(static_cast<std::size_t>(lda * n + 2), base::Value<T>(-83, 29)),
        full(static_cast<std::size_t>(n * n)) {
    for (int j = 0; j < n; ++j) {
      for (int i = j; i < n; ++i) {
        T value = i == j ? base::Value<T>(4 + i % 3, he ? 0 : .25L)
                         : base::Value<T>(((i + j) % 5 - 2) / 1024.L,
                                          ((i + 2 * j) % 5 - 2) / 2048.L);
        if (n >= 3 && i == 0 && j == 0) {
          value = T{};
        }
        if (n >= 3 && j == 0 && (i == 1 || i == 2)) {
          value = base::Value<T>(i, i / 8.L);
        }
        if (singular && i == n - 1) {
          value = T{};
        }
        full[i * n + j] = base::ToWide(value);
        full[j * n + i] = base::Adjoint(full[i * n + j], he);
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (Selected(i, j)) {
          const auto value = full[i * n + j];
          a[1 + j * lda + i] = base::Value<T>(value.real(), value.imag());
        }
      }
    }
    before = a;
  }

  [[nodiscard]] bool Selected(int i, int j) const {
    return upper ? i <= j : i >= j;
  }

  void Guards(base::TestContext& test) const {
    ASC_DENSE_TEST_EQ(test, a.front(), before.front());
    ASC_DENSE_TEST_EQ(test, a.back(), before.back());
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < lda; ++i) {
        if (i >= n || !Selected(i, j)) {
          ASC_DENSE_TEST_EQ(test, a[1 + j * lda + i], before[1 + j * lda + i]);
        }
      }
    }
  }
};
}  // namespace asc_aasen_two_stage_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_FIXTURE_H_
