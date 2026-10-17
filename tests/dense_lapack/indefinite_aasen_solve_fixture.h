#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>

#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_fixture.h"
#include "indefinite_aasen_native.h"
#include "indefinite_rook_test_support.h"
namespace asc_aasen_solve_test {
namespace base = asc_indefinite_rook_test;
template <typename T>
struct Sample : asc_aasen_test::Sample<T> {
  int nrhs;
  asc::DenseBlasLayout rhs_layout;
  std::array<T, 350> b{};
  std::array<T, 350> original_b{};
  std::array<base::Wide, 201> solution{};

  Sample(int n, int columns, bool he, asc::DenseBlasTriangle triangle,
         asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_storage,
         int exponent, bool singular)
      : asc_aasen_test::Sample<T>(n, he, triangle, layout, 0, false),
        nrhs(columns),
        rhs_layout(rhs_storage) {
    const auto scale = std::ldexp(typename Sample::Real{1}, exponent);
    // A well-conditioned Hermitian/transpose-symmetric block forces a
    // nontrivial positive interchange. The remaining diagonal block is exact.
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j <= i; ++j) {
        T value = i == j ? base::Value<T>(4, he ? 0 : 0.25L) : T{};
        if (n >= 3 && j == 0) {
          if (i == 0) {
            value = T{};
          }
          if (i == 1) {
            value = base::Value<T>(1, 0.125L);
          }
          if (i == 2) {
            value = base::Value<T>(2, 0.25L);
          }
        }
        if (singular && (i == n - 1 || j == n - 1)) {
          value = T{};
        }
        const auto wide = base::ToWide(value * scale);
        this->full[i * n + j] = wide;
        this->full[j * n + i] = base::Adjoint(wide, he);
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (this->Selected(i, j)) {
          const auto value = this->full[i * n + j];
          this->a[this->Offset(i, j)] =
              base::Value<T>(value.real(), value.imag());
        }
      }
    }
    this->original = this->a;
    b.fill(base::Value<T>(-751, 19));
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        solution[j * n + i] = base::ToWide(base::Value<T>(
            static_cast<long double>((i + j) % 3 + 1) / 4, 0.125L));
      }
      for (int i = 0; i < n; ++i) {
        base::Wide value{};
        for (int k = 0; k < n; ++k) {
          value += this->full[i * n + k] * solution[j * n + k];
        }
        b[BOffset(i, j)] = base::Value<T>(value.real(), value.imag());
      }
    }
    original_b = b;
  }
  [[nodiscard]] int Ldb() const {
    return (rhs_layout == base::kColumn ? this->n : nrhs) + 2;
  }
  [[nodiscard]] std::size_t BOffset(int i, int j) const {
    return 1U + (rhs_layout == base::kColumn
                     ? static_cast<std::size_t>(j) * Ldb() + i
                     : static_cast<std::size_t>(i) * Ldb() + j);
  }
  auto Rhs() { return base::Matrix(b, this->n, nrhs, rhs_layout, Ldb()); }
  void RhsGuards(base::TestContext& test) const {
    for (std::size_t k = 0; k < b.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i =
          rhs_layout == base::kColumn ? relative % Ldb() : relative / Ldb();
      const int j =
          rhs_layout == base::kColumn ? relative / Ldb() : relative % Ldb();
      if (k == 0 || i >= this->n || j >= nrhs) {
        ASC_DENSE_TEST_CHECK(
            test, base::EqualBytes(&b[k], &original_b[k], sizeof(T)));
      }
    }
  }
  void Solution(base::TestContext& test) const {
    long double error = 0;
    long double residual = 0;
    long double norm_a = 0;
    long double norm_x = 0;
    long double norm_b = 0;
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < this->n; ++i) {
        const auto x = base::ToWide(b[BOffset(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(x.real()) && std::isfinite(x.imag()));
        error = std::max(error, std::abs(x - solution[j * this->n + i]));
        norm_x = std::max(norm_x, std::abs(solution[j * this->n + i]));
        base::Wide product{};
        long double row_norm = 0;
        for (int k = 0; k < this->n; ++k) {
          const auto a = this->full[i * this->n + k];
          product += a * base::ToWide(b[BOffset(k, j)]);
          row_norm += std::abs(a);
        }
        norm_a = std::max(norm_a, row_norm);
        const auto rhs = base::ToWide(original_b[BOffset(i, j)]);
        norm_b = std::max(norm_b, std::abs(rhs));
        residual = std::max(residual, std::abs(product - rhs));
      }
    }
    const auto tolerance =
        64 * std::max(this->n, 1) *
        std::numeric_limits<typename Sample::Real>::epsilon();
    ASC_DENSE_TEST_CHECK(test, error <= tolerance * norm_x);
    ASC_DENSE_TEST_CHECK(test,
                         residual <= tolerance * (norm_a * norm_x + norm_b));
  }
};
template <typename T>
void Produce(base::TestContext& test, Sample<T>& sample) {
  // The fixture is generated column-major for this direct prerequisite call.
  // Public row/column producer reuse is exercised in checked consumer tests.
  ASC_DENSE_TEST_EQ(test, sample.layout, base::kColumn);
  if (sample.n == 0) {
    return;
  }
  std::array<T, 4360> work{};
  std::array<lapack_int, 69> pivots{};
  lapack_int info = std::numeric_limits<lapack_int>::min();
  asc_aasen_test::Native(sample.hermitian,
                         sample.triangle == base::kUpper ? 'U' : 'L', sample.n,
                         sample.a.data() + 1, sample.Ld(), pivots.data() + 1,
                         work.data(), std::max(1, 65 * sample.n), info);
  ASC_DENSE_TEST_EQ(test, info, 0);
  for (int i = 0; i < sample.n; ++i) {
    sample.pivots[i + 1] = pivots[i + 1];
  }
  sample.Reconstruction(test);
  sample.Guards(test);
}
}  // namespace asc_aasen_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_FIXTURE_H_
