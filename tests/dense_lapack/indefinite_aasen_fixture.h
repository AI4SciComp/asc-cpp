#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <utility>

#include "indefinite_rk_fixture.h"
#include "indefinite_rook_test_support.h"
namespace asc_aasen_test {
namespace base = asc_indefinite_rook_test;
template <typename T>
struct Sample : asc_rk_test::Sample<T> {
  using asc_rk_test::Sample<T>::Sample;
  void Reconstruction(base::TestContext& test) const {
    // Extract the shifted unit triangular factor as a lower factor F. For
    // upper storage F = U**T (symmetric) or U**H (Hermitian). Form F*T*F**H/T
    // in wider arithmetic and undo the recorded symmetric swaps in reverse.
    const int n = this->n;
    const bool he = this->hermitian;
    const bool upper = this->triangle == base::kUpper;
    using base::Wide;
    std::array<Wide, 4489> f{};
    std::array<Wide, 4489> ft{};
    std::array<Wide, 4489> product{};
    for (int i = 0; i < n; ++i) {
      f[i * n + i] = Wide{1, 0};
      for (int j = 1; j < i; ++j) {
        f[i * n + j] = upper ? base::Adjoint(this->Entry(j - 1, i), he)
                             : this->Entry(i, j - 1);
      }
    }
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        ft[i * n + j] = f[i * n + j] * this->Entry(j, j);
        if (j > 0) {
          const auto t = upper ? this->Entry(j - 1, j)
                               : base::Adjoint(this->Entry(j, j - 1), he);
          ft[i * n + j] += f[i * n + j - 1] * t;
        }
        if (j + 1 < n) {
          const auto t = upper ? base::Adjoint(this->Entry(j, j + 1), he)
                               : this->Entry(j + 1, j);
          ft[i * n + j] += f[i * n + j + 1] * t;
        }
      }
    }
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        for (int k = 0; k < n; ++k) {
          product[i * n + j] += ft[i * n + k] * base::Adjoint(f[j * n + k], he);
        }
      }
    }
    for (int i = n - 1; i >= 0; --i) {
      const auto pivot = this->pivots[i + 1];
      ASC_DENSE_TEST_CHECK(test, pivot >= i + 1 && pivot <= n);
      if (pivot < i + 1 || pivot > n) {
        return;
      }
      const int p = static_cast<int>(pivot) - 1;
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
      error = std::max(error, std::abs(product[i] - this->full[i]));
      norm = std::max(norm, std::abs(this->full[i]));
    }
    const auto tolerance =
        32 * std::max(n, 1) *
        std::numeric_limits<typename Sample::Real>::epsilon() * norm;
    if (!(error <= tolerance)) {
      std::fprintf(
          stderr,
          "Aasen reconstruction n=%d he=%d upper=%d error=%Lg limit=%Lg\n", n,
          he, upper, error, tolerance);
    }
    ASC_DENSE_TEST_CHECK(test, error <= tolerance);
  }
};
}  // namespace asc_aasen_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_FIXTURE_H_
