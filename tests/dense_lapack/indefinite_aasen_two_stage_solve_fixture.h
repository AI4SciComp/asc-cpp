#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <vector>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "indefinite_aasen_two_stage_fixture.h"
#include "indefinite_aasen_two_stage_native.h"
#include "indefinite_rook_test_support.h"
namespace asc_aasen_two_stage_solve_test {
namespace base = asc_indefinite_rook_test;
namespace factor = asc_aasen_two_stage_test;
template <typename T>
struct Sample {
  factor::Sample<T> original;
  int n;
  int nrhs;
  int ltb;
  int work_entries;
  int ldb;
  bool row;
  bool rhs_row;
  bool singular;
  std::vector<T> a;
  std::vector<T> tb;
  std::vector<asc::index_t> p;
  std::vector<asc::index_t> q;
  std::vector<T> b;
  std::vector<T> before_b;
  std::vector<base::Wide> solution;

  Sample(int order, int columns, bool he, bool upper, bool row_a, bool row_b,
         int band_mode, int work_mode, bool is_singular = false)
      : original(order, he, upper, is_singular),
        n(order),
        nrhs(columns),
        ltb(order * std::array{4, 10, 577}[band_mode]),
        work_entries(order * std::array{1, 3, 192}[work_mode]),
        ldb((row_b ? columns : order) + 3),
        row(row_a),
        rhs_row(row_b),
        singular(is_singular),
        a(original.a),
        tb(static_cast<std::size_t>(ltb + 2), base::Value<T>(-73, 11)),
        p(static_cast<std::size_t>(order + 2), -71),
        q(p),
        b(static_cast<std::size_t>((row_b ? order : columns) * ldb + 2),
          base::Value<T>(-79, 13)),
        solution(static_cast<std::size_t>(order * columns)) {
    MakeRhs();
  }
  [[nodiscard]] std::size_t AOffset(int i, int j) const {
    return 1U + static_cast<std::size_t>(row ? i * original.lda + j
                                             : j * original.lda + i);
  }
  [[nodiscard]] std::size_t BOffset(int i, int j) const {
    return 1U + static_cast<std::size_t>(rhs_row ? i * ldb + j : j * ldb + i);
  }
  void MakeRhs() {
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        solution[j * n + i] =
            base::ToWide(base::Value<T>(((i + j) % 3 + 1) / 4.L, 0.125L));
      }
      for (int i = 0; i < n; ++i) {
        base::Wide value{};
        for (int k = 0; k < n; ++k) {
          value += original.full[i * n + k] * solution[j * n + k];
        }
        b[BOffset(i, j)] = base::Value<T>(value.real(), value.imag());
      }
    }
    before_b = b;
  }
  void Scale(int exponent) {
    const auto scale = std::ldexp(asc::DenseBlasRealType<T>{1}, exponent);
    for (auto& x : original.full) {
      x *= scale;
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (original.Selected(i, j)) {
          original.a[1 + j * original.lda + i] *= scale;
        }
      }
    }
    original.before = original.a;
    a = original.a;
    MakeRhs();
  }
  void Produce(base::TestContext& test, bool reconstruct) {
    if (n == 0) {
      return;
    }
    std::vector<T> work(static_cast<std::size_t>(work_entries + 2), T{-89});
    std::vector<lapack_int> np(static_cast<std::size_t>(n + 2), -71);
    auto nq = np;
    lapack_int info = std::numeric_limits<lapack_int>::min();
    factor::Native(original.hermitian, original.upper ? 'U' : 'L', n,
                   original.a.data() + 1, original.lda, tb.data() + 1, ltb,
                   np.data() + 1, nq.data() + 1, work.data() + 1, work_entries,
                   info);
    ASC_DENSE_TEST_EQ(test, info, singular ? n : 0);
    ASC_DENSE_TEST_EQ(test, np.front(), -71);
    ASC_DENSE_TEST_EQ(test, np.back(), -71);
    ASC_DENSE_TEST_EQ(test, nq.front(), -71);
    ASC_DENSE_TEST_EQ(test, nq.back(), -71);
    ASC_DENSE_TEST_EQ(test, work.front(), T{-89});
    ASC_DENSE_TEST_EQ(test, work.back(), T{-89});
    for (int i = 0; i < n; ++i) {
      p[i + 1] = np[i + 1];
      q[i + 1] = nq[i + 1];
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        a[AOffset(i, j)] = original.a[1 + j * original.lda + i];
      }
    }
    if (reconstruct) {
      const int nb = std::min({192, (ltb / n - 1) / 3, work_entries / n});
      factor::Reconstruction(test, n, original.hermitian, original.upper,
                             original.a.data() + 1, original.lda, tb.data() + 1,
                             ltb / n, nb, p.data() + 1, q.data() + 1,
                             original.full);
    }
    original.Guards(test);
    ASC_DENSE_TEST_EQ(test, tb.front(), base::Value<T>(-73, 11));
    ASC_DENSE_TEST_EQ(test, tb.back(), base::Value<T>(-73, 11));
  }
  void RhsGuards(base::TestContext& test) const {
    for (std::size_t k = 0; k < b.size(); ++k) {
      const int relative = static_cast<int>(k) - 1;
      const int i = rhs_row ? relative / ldb : relative % ldb;
      const int j = rhs_row ? relative % ldb : relative / ldb;
      if (k == 0 || i >= n || j >= nrhs) {
        ASC_DENSE_TEST_CHECK(test,
                             base::EqualBytes(&b[k], &before_b[k], sizeof(T)));
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
      for (int i = 0; i < n; ++i) {
        const auto x = base::ToWide(b[BOffset(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(x.real()) && std::isfinite(x.imag()));
        error = std::max(error, std::abs(x - solution[j * n + i]));
        norm_x = std::max(norm_x, std::abs(solution[j * n + i]));
        base::Wide product{};
        long double row_norm = 0;
        for (int k = 0; k < n; ++k) {
          const auto value = original.full[i * n + k];
          product += value * base::ToWide(b[BOffset(k, j)]);
          row_norm += std::abs(value);
        }
        norm_a = std::max(norm_a, row_norm);
        const auto rhs = base::ToWide(before_b[BOffset(i, j)]);
        norm_b = std::max(norm_b, std::abs(rhs));
        residual = std::max(residual, std::abs(product - rhs));
      }
    }
    const long double tolerance =
        64 * std::max(n, 1) *
        std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    ASC_DENSE_TEST_CHECK(test, error <= tolerance * norm_x);
    ASC_DENSE_TEST_CHECK(test,
                         residual <= tolerance * (norm_a * norm_x + norm_b));
  }
};
}  // namespace asc_aasen_two_stage_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_FIXTURE_H_
