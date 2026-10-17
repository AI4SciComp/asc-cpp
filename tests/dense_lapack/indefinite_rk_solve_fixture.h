#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>

#include "../dense/test_support.h"
#include "asc/dense/blas.h"
#include "indefinite_rk_fixture.h"
#include "indefinite_rk_test_support.h"
namespace asc_rk_solve_test {
using asc_rk_test::EqualBytes;
using asc_rk_test::kColumn;
using asc_rk_test::Matrix;
using asc_rk_test::Sample;
using asc_rk_test::TestContext;
using asc_rk_test::ToWide;
using asc_rk_test::Value;
using asc_rk_test::Wide;
template <typename T>
struct RightHandSides {
  int n;
  int nrhs;
  asc::DenseBlasLayout layout;
  std::array<T, 400> values{};
  std::array<T, 400> before{};
  std::array<Wide, 201> expected{};
  RightHandSides(const Sample<T>& sample, int columns,
                 asc::DenseBlasLayout storage)
      : n(sample.n), nrhs(columns), layout(storage) {
    values.fill(Value<T>(-701, 31));
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        expected[j * n + i] = ToWide(
            Value<T>(1 + static_cast<long double>((i + 2 * j) % 5) / 8,
                     static_cast<long double>((2 * i + j) % 3 - 1) / 16));
      }
      for (int i = 0; i < n; ++i) {
        Wide sum{};
        for (int k = 0; k < n; ++k) {
          sum += sample.full[i * n + k] * expected[j * n + k];
        }
        values[Offset(i, j)] = Value<T>(sum.real(), sum.imag());
      }
    }
    before = values;
  }
  [[nodiscard]] int Ld() const { return layout == kColumn ? n + 2 : nrhs + 2; }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + (layout == kColumn ? static_cast<std::size_t>(j) * Ld() + i
                                   : static_cast<std::size_t>(i) * Ld() + j);
  }
  auto View() { return Matrix(values, n, nrhs, layout, Ld()); }
  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < values.size(); ++k) {
      bool selected = false;
      for (int j = 0; j < nrhs; ++j) {
        for (int i = 0; i < n; ++i) {
          selected = selected || k == Offset(i, j);
        }
      }
      if (!selected) {
        ASC_DENSE_TEST_CHECK(test,
                             EqualBytes(&values[k], &before[k], sizeof(T)));
      }
    }
  }
  void Verify(TestContext& test, const Sample<T>& sample) const {
    const long double epsilon =
        std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        const Wide value = ToWide(values[Offset(i, j)]);
        ASC_DENSE_TEST_CHECK(
            test, std::isfinite(value.real()) && std::isfinite(value.imag()));
        ASC_DENSE_TEST_CHECK(test, std::abs(value - expected[j * n + i]) <=
                                       128 * std::max(1, n) * epsilon *
                                           std::abs(expected[j * n + i]));
        Wide sum{};
        long double magnitude = std::abs(ToWide(before[Offset(i, j)]));
        for (int k = 0; k < n; ++k) {
          const Wide term =
              sample.full[i * n + k] * ToWide(values[Offset(k, j)]);
          sum += term;
          magnitude += std::abs(term);
        }
        ASC_DENSE_TEST_CHECK(test,
                             std::abs(sum - ToWide(before[Offset(i, j)])) <=
                                 128 * std::max(1, n) * epsilon * magnitude);
      }
    }
  }
};
}  // namespace asc_rk_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_FIXTURE_H_
