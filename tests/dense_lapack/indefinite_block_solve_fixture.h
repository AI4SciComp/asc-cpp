#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "indefinite_test_support.h"
namespace asc_block_solve_test {
using asc_indefinite_test::Adjoint;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Matrix;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::ToWide;
using asc_indefinite_test::Value;
using asc_indefinite_test::Wide;
template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 5000> a{};
  std::array<T, 5000> original{};
  std::array<asc::index_t, 72> pivots{};
  std::array<Wide, 4489> full{};

  Sample(int order, bool he, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout storage, int exponent, bool singular)
      : n(order), hermitian(he), triangle(tri), layout(storage) {
    a.fill(Value<T>(-503, 29));
    pivots.fill(-509);
    const Real scale = std::ldexp(Real{1}, exponent);
    auto permutation = [order](int i) {
      if (order > 3) {
        if (i == 1) {
          return order - 1;
        }
        if (i == order - 1) {
          return 1;
        }
      }
      return i;
    };
    for (int j = 0; j < n; ++j) {
      for (int i = j; i < n; ++i) {
        const int p = permutation(i);
        const int q = permutation(j);
        const int low = std::min(p, q);
        const int high = std::max(p, q);
        T value{};
        if (p == q) {
          value = Value<T>(p % 3 == 0 || p == n - 1 ? 4 : 0,
                           hermitian ? 0 : 0.125L);
        } else if (low % 3 == 1 && high == low + 1) {
          value = Value<T>(2, 0.5L);
        } else {
          value = Value<T>(static_cast<long double>((p + q) % 3 - 1) / 128,
                           static_cast<long double>((p + q) % 5 - 2) / 256);
        }
        if (order == 3) {
          // A distant largest off-diagonal entry forces a classic 2x2
          // pivot with a nontrivial single interchange in either triangle.
          const long double magnitude = high - low == 2 ? 4 : 1;
          value = p == q ? T{} : Value<T>(magnitude, 0.125L);
        }
        value *= scale;
        const Wide wide = ToWide(value);
        full[i * n + j] = wide;
        full[j * n + i] = Adjoint(wide, hermitian);
      }
    }
    if (singular) {
      for (int i = 0; i < n; ++i) {
        full[i * n + n - 1] = Wide{};
        full[(n - 1) * n + i] = Wide{};
      }
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (Selected(i, j)) {
          a[Offset(i, j)] =
              Value<T>(full[i * n + j].real(), full[i * n + j].imag());
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian && i == j) {
              a[Offset(i, j)].imag(std::numeric_limits<Real>::quiet_NaN());
            }
          }
        } else {
          a[Offset(i, j)] = Value<T>(std::numeric_limits<Real>::quiet_NaN());
        }
      }
    }
    original = a;
  }

  [[nodiscard]] int Ld() const { return n + 2; }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1U + (layout == kColumn ? static_cast<std::size_t>(j) * Ld() + i
                                   : static_cast<std::size_t>(i) * Ld() + j);
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == kUpper ? i <= j : i >= j;
  }
  auto View() { return Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] auto ConstView() const { return Matrix(a, n, n, layout, Ld()); }
  [[nodiscard]] Wide Entry(int i, int j) const {
    return ToWide(a[Offset(i, j)]);
  }

  void Guards(TestContext& test) const {
    for (std::size_t k = 0; k < a.size(); ++k) {
      const auto relative = static_cast<int>(k) - 1;
      const int i = layout == kColumn ? relative % Ld() : relative / Ld();
      const int j = layout == kColumn ? relative / Ld() : relative % Ld();
      if (k == 0 || i >= n || j >= n || !Selected(i, j)) {
        ASC_DENSE_TEST_CHECK(test, EqualBytes(&a[k], &original[k], sizeof(T)));
      }
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -509);
    for (std::size_t i = static_cast<std::size_t>(n) + 1; i < pivots.size();
         ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[i], -509);
    }
  }
};

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
}  // namespace asc_block_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_FIXTURE_H_
