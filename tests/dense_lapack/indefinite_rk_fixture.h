#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <utility>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "indefinite_rk_test_support.h"
namespace asc_rk_test {
template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  bool hermitian;
  asc::DenseBlasTriangle triangle;
  asc::DenseBlasLayout layout;
  std::array<T, 5000> a{};
  std::array<T, 5000> original{};
  std::array<T, 72> e{};
  std::array<asc::index_t, 72> pivots{};
  std::array<Wide, 4489> full{};

  Sample(int order, bool he, asc::DenseBlasTriangle tri,
         asc::DenseBlasLayout storage, int exponent, bool singular)
      : n(order), hermitian(he), triangle(tri), layout(storage) {
    a.fill(Value<T>(-503, 29));
    pivots.fill(-509);
    e.fill(Value<T>(-511, 17));
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
          // Force the rook search to leave the current column and choose an
          // internal 2x2 block, requiring BOTH independent interchanges.
          const bool strong = triangle == kUpper ? high == 1 : low == 1;
          long double magnitude = 1;
          if (strong) {
            magnitude = 4;
          } else if (high - low == 2) {
            magnitude = 0.5L;
          }
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
    ASC_DENSE_TEST_EQ(test, e.front(), Value<T>(-511, 17));
    for (std::size_t i = static_cast<std::size_t>(n) + 1; i < pivots.size();
         ++i) {
      ASC_DENSE_TEST_EQ(test, pivots[i], -509);
      ASC_DENSE_TEST_EQ(test, e[i], Value<T>(-511, 17));
    }
  }

  void Reconstruction(TestContext& test) const {
    // Form the unit triangular factor and explicit block D, multiply in wide
    // arithmetic, then undo the global permutation. This uses no factor solve
    // and no source update recurrence as a numerical oracle.
    std::array<Wide, 4489> triangular{};
    std::array<Wide, 4489> td{};
    std::array<Wide, 4489> reconstructed{};
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        if (i == j) {
          triangular[i * n + j] = Wide{1, 0};
        } else if (Selected(i, j)) {
          triangular[i * n + j] = Entry(i, j);
        }
      }
    }
    for (int j = 0; j < n;) {
      const bool paired = pivots[static_cast<std::size_t>(j) + 1] < 0;
      for (int i = 0; i < n; ++i) {
        td[i * n + j] = triangular[i * n + j] * Entry(j, j);
        if (paired) {
          const Wide selected = ToWide(
              e[static_cast<std::size_t>(triangle == kUpper ? j + 2 : j + 1)]);
          const Wide upper =
              triangle == kUpper ? selected : Adjoint(selected, hermitian);
          td[i * n + j] +=
              triangular[i * n + j + 1] * Adjoint(upper, hermitian);
          td[i * n + j + 1] = triangular[i * n + j] * upper +
                              triangular[i * n + j + 1] * Entry(j + 1, j + 1);
        }
      }
      j += paired ? 2 : 1;
    }
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        for (int k = 0; k < n; ++k) {
          reconstructed[i * n + j] +=
              td[i * n + k] * Adjoint(triangular[j * n + k], hermitian);
        }
      }
    }
    for (int step = 0; step < n; ++step) {
      const int i = triangle == kUpper ? step : n - 1 - step;
      const auto raw = pivots[static_cast<std::size_t>(i) + 1];
      const int target = static_cast<int>(raw < 0 ? -raw : raw) - 1;
      ASC_DENSE_TEST_CHECK(test, target >= 0 && target < n);
      if (target < 0 || target >= n) {
        return;
      }
      for (int j = 0; j < n; ++j) {
        std::swap(reconstructed[i * n + j], reconstructed[target * n + j]);
      }
      for (int j = 0; j < n; ++j) {
        std::swap(reconstructed[j * n + i], reconstructed[j * n + target]);
      }
    }
    long double error = 0;
    long double norm = 0;
    for (int i = 0; i < n * n; ++i) {
      ASC_DENSE_TEST_CHECK(test, std::isfinite(reconstructed[i].real()) &&
                                     std::isfinite(reconstructed[i].imag()));
      error = std::max(error, std::abs(reconstructed[i] - full[i]));
      norm = std::max(norm, std::abs(full[i]));
    }
    const auto tolerance =
        32 * std::max(n, 1) * std::numeric_limits<Real>::epsilon() * norm;
    if (!(error <= tolerance)) {
      std::fprintf(
          stderr,
          "RK reconstruction n=%d he=%d tri=%d layout=%d error=%Lg limit=%Lg\n",
          n, hermitian, static_cast<int>(triangle), static_cast<int>(layout),
          error, tolerance);
    }
    ASC_DENSE_TEST_CHECK(test, error <= tolerance);
  }
};
}  // namespace asc_rk_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_FIXTURE_H_
