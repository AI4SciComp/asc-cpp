#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_FIXTURE_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_FIXTURE_H_
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <utility>

#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "indefinite_test_support.h"
namespace asc_packed_indefinite_test {
using asc_indefinite_test::Adjoint;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kHost;
using asc_indefinite_test::kLower;
using asc_indefinite_test::kRow;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Take;
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
         asc::DenseBlasLayout storage, int exponent)
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
        value *= scale;
        const Wide wide = ToWide(value);
        full[i * n + j] = wide;
        full[j * n + i] = Adjoint(wide, hermitian);
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
        }
      }
    }
    original = a;
  }

  [[nodiscard]] std::size_t Offset(int i, int j) const {
    // Test-owned formulas; no production packing helper is used as an oracle.
    int k;
    if (layout == kColumn) {
      k = triangle == kUpper ? j * (j + 1) / 2 + i
                             : j * n - j * (j - 1) / 2 + i - j;
    } else {
      k = triangle == kLower ? i * (i + 1) / 2 + j
                             : i * n - i * (i - 1) / 2 + j - i;
    }
    return 1U + static_cast<std::size_t>(k);
  }
  [[nodiscard]] bool Selected(int i, int j) const {
    return triangle == kUpper ? i <= j : i >= j;
  }
  auto View() {
    return Take(asc::DenseBlasPackedMatrixView<T>::Create(
        a.data() + 1, n, layout, {a.data(), sizeof(a), kHost}));
  }
  [[nodiscard]] auto ConstView() const {
    return Take(asc::DenseBlasPackedMatrixView<const T>::Create(
        a.data() + 1, n, layout, {a.data(), sizeof(a), kHost}));
  }
  [[nodiscard]] Wide Entry(int i, int j) const {
    return ToWide(a[Offset(i, j)]);
  }
  void Reset(int kind) {
    if (kind == 0) {
      return;
    }
    for (int j = 0; j < n; ++j) {
      for (int i = 0; i < n; ++i) {
        if (kind == 1 || (kind == 2 && (i == n / 2 || j == n / 2))) {
          full[i * n + j] = {};
        } else if (kind == 3) {
          full[i * n + j] = i == j ? Wide{4, 0} : Wide{};
          if (n >= 2 && i < 2 && j < 2) {
            const Wide off = ToWide(Value<T>(2, 0.5L));
            const Wide off_diagonal = i > j ? off : Adjoint(off, hermitian);
            full[i * n + j] = i == j ? Wide{} : off_diagonal;
          }
        }
        if (Selected(i, j)) {
          a[Offset(i, j)] =
              Value<T>(full[i * n + j].real(), full[i * n + j].imag());
          if constexpr (asc::DenseBlasComplex<T>) {
            if (hermitian && i == j) {
              a[Offset(i, j)].imag(std::numeric_limits<Real>::quiet_NaN());
            }
          }
        }
      }
    }
    original = a;
  }
  void Guards(TestContext& test) const {
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(&a.front(), &original.front(), sizeof(T)));
    for (std::size_t k = static_cast<std::size_t>(n * (n + 1) / 2) + 1;
         k < a.size(); ++k) {
      ASC_DENSE_TEST_CHECK(test, EqualBytes(&a[k], &original[k], sizeof(T)));
    }
    ASC_DENSE_TEST_EQ(test, pivots.front(), -509);
    for (std::size_t k = static_cast<std::size_t>(n) + 1; k < pivots.size();
         ++k) {
      ASC_DENSE_TEST_EQ(test, pivots[k], -509);
    }
  }

  void Reconstruction(TestContext& test) const {
    // Independent reverse Schur-complement reconstruction. Restore a complete
    // 1x1/2x2 block, multiply stored multipliers by D, add LDU/LDL* updates,
    // then undo its simultaneous row/column interchange. No LAPACK solve or
    // source implementation is used as the oracle.
    std::array<Wide, 4489> reconstructed{};
    int cursor = triangle == kLower ? n - 1 : 0;
    while (cursor >= 0 && cursor < n) {
      const bool paired = pivots[static_cast<std::size_t>(cursor) + 1] < 0;
      const int size = paired ? 2 : 1;
      const int first = triangle == kLower ? cursor - size + 1 : cursor;
      const int last = first + size;
      std::array<Wide, 4> d{};
      d[0] = Entry(first, first);
      if (paired) {
        d[3] = Entry(first + 1, first + 1);
        if (triangle == kLower) {
          d[2] = Entry(first + 1, first);
          d[1] = Adjoint(d[2], hermitian);
        } else {
          d[1] = Entry(first, first + 1);
          d[2] = Adjoint(d[1], hermitian);
        }
      }
      const int begin_active = triangle == kLower ? last : 0;
      const int end_active = triangle == kLower ? n : first;
      for (int i = begin_active; i < end_active; ++i) {
        for (int j = begin_active; j < end_active; ++j) {
          for (int p = 0; p < size; ++p) {
            for (int q = 0; q < size; ++q) {
              reconstructed[i * n + j] +=
                  Entry(i, first + p) * d[2 * p + q] *
                  Adjoint(Entry(j, first + q), hermitian);
            }
          }
        }
        for (int p = 0; p < size; ++p) {
          Wide value{};
          for (int q = 0; q < size; ++q) {
            value += Entry(i, first + q) * d[2 * q + p];
          }
          reconstructed[i * n + first + p] = value;
          reconstructed[(first + p) * n + i] = Adjoint(value, hermitian);
        }
      }
      for (int p = 0; p < size; ++p) {
        for (int q = 0; q < size; ++q) {
          reconstructed[(first + p) * n + first + q] = d[2 * p + q];
        }
      }
      const int swapped = triangle == kLower ? last - 1 : first;
      const auto raw = pivots[static_cast<std::size_t>(cursor) + 1];
      const int target = static_cast<int>(paired ? -raw : raw) - 1;
      for (int j = 0; j < n; ++j) {
        std::swap(reconstructed[swapped * n + j],
                  reconstructed[target * n + j]);
      }
      for (int i = 0; i < n; ++i) {
        std::swap(reconstructed[i * n + swapped],
                  reconstructed[i * n + target]);
      }
      cursor += triangle == kLower ? -size : size;
    }
    CheckReconstruction(test, reconstructed);
  }

  void CheckReconstruction(TestContext& test,
                           const std::array<Wide, 4489>& reconstructed) const {
    long double error = 0;
    long double norm = 0;
    for (int i = 0; i < n * n; ++i) {
      const long double difference = std::abs(reconstructed[i] - full[i]);
      ASC_DENSE_TEST_CHECK(test, std::isfinite(difference));
      if (!std::isfinite(difference)) {
        error = std::numeric_limits<long double>::infinity();
      } else {
        error = std::max(error, difference);
      }
      norm = std::max(norm, std::abs(full[i]));
    }
    const auto tolerance =
        32 * std::max(n, 1) * std::numeric_limits<Real>::epsilon() * norm;
    if (!(error <= tolerance)) {
      std::fprintf(stderr,
                   "reconstruction n=%d he=%d triangle=%d layout=%d "
                   "error=%Lg limit=%Lg\n",
                   n, static_cast<int>(hermitian), static_cast<int>(triangle),
                   static_cast<int>(layout), error, tolerance);
    }
    ASC_DENSE_TEST_CHECK(test, error <= tolerance);
    Guards(test);
  }
};

}  // namespace asc_packed_indefinite_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_FIXTURE_H_
