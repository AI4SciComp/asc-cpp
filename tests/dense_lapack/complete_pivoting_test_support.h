#ifndef ASC_TESTS_DENSE_LAPACK_COMPLETE_PIVOTING_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_COMPLETE_PIVOTING_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>

#include "../../src/dense/lapack/internal_complete_pivoting_prototypes.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_complete_pivoting.h"
#include "tridiagonal_test_support.h"  // IWYU pragma: export

namespace asc_complete_pivoting_test {
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::ToWide;
using asc_tridiagonal_test::Value;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::Wide;
using asc_tridiagonal_test::WithoutAllocation;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kScratch = asc_tridiagonal_test::kScratch;
constexpr auto kInteger = asc_tridiagonal_test::kInteger;

template <typename T>
bool Same(const T& a, const T& b) {
  const auto x = std::as_bytes(std::span(&a, 1));
  const auto y = std::as_bytes(std::span(&b, 1));
  return std::equal(x.begin(), x.end(), y.begin());
}

template <typename T>
T Coefficient(asc::extent_t n, asc::extent_t i, asc::extent_t j,
              asc::extent_t seed, int exponent) {
  const auto row = (i + seed) % n;
  const auto column = (n - j + seed) % n;
  const auto real =
      row == column
          ? static_cast<long double>(4 * n + row + 1)
          : static_cast<long double>((row + 2 * column + seed) % 7 - 3) / 8;
  const auto imaginary =
      static_cast<long double>((2 * row + column + seed) % 5 - 2) / 16;
  return Value<T>(std::scalbn(real, exponent),
                  std::scalbn(imaginary, exponent));
}

template <typename T>
struct Problem {
  using Real = asc::DenseBlasRealType<T>;
  std::array<T, 160> dense;
  std::array<T, 10> rhs;
  std::array<asc::index_t, 10> rows;
  std::array<asc::index_t, 10> columns;
  std::array<Real, 3> scale{Real{-71}, Real{-73}, Real{-79}};
  asc::extent_t n;
  asc::extent_t leading;
  asc::DenseBlasLayout layout;
  bool zero;
  Problem(asc::extent_t order, asc::extent_t padding,
          asc::DenseBlasLayout storage, asc::extent_t seed, int exponent,
          bool singular = false, int rhs_kind = 1)
      : n(order),
        leading(std::max(asc::extent_t{1}, order) + padding),
        layout(storage),
        zero(singular) {
    dense.fill(Value<T>(-83, 23));
    rhs.fill(Value<T>(-89, 29));
    rows.fill(-97);
    columns.fill(-101);
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        At(i, j) = zero ? T{} : Coefficient<T>(n, i, j, seed, exponent);
      }
    }
    for (asc::extent_t i = 0; i < n; ++i) {
      Wide value{};
      if (zero) {
        Real base = Real{1};
        if (rhs_kind == 0) {
          base = Small();
        } else if (rhs_kind != 1) {
          base = std::numeric_limits<Real>::max() / Real{32};
        }
        const Real real = base * static_cast<Real>(i + 1);
        value = ToWide(Value<T>(real, real / 2));
      } else {
        for (asc::extent_t j = 0; j < n; ++j) {
          value += ToWide(At(i, j)) * Exact(j);
        }
      }
      rhs[static_cast<std::size_t>(i + 1)] =
          Value<T>(value.real(), value.imag());
    }
  }
  static Real Small() {
    return std::numeric_limits<Real>::min() /
           std::numeric_limits<Real>::epsilon();
  }
  static Wide Exact(asc::extent_t j) {
    return ToWide(Value<T>(static_cast<long double>(j + 1) / 8,
                           static_cast<long double>(j + 2) / 16));
  }
  [[nodiscard]] std::size_t Index(asc::extent_t i, asc::extent_t j) const {
    return static_cast<std::size_t>(
        1 + (layout == kColumn ? j * leading + i : i * leading + j));
  }
  T& At(asc::extent_t i, asc::extent_t j) { return dense[Index(i, j)]; }
  [[nodiscard]] T At(asc::extent_t i, asc::extent_t j) const {
    return dense[Index(i, j)];
  }
  auto Matrix() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        dense.data() + 1, n, n, layout, leading,
        {dense.data(), sizeof(dense), kHost}));
  }
  [[nodiscard]] auto Matrix() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        dense.data() + 1, n, n, layout, leading,
        {dense.data(), sizeof(dense), kHost}));
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    return asc::QueryGetc2Workspace(provider, Matrix(), Vector(rows, n),
                                    Vector(columns, n));
  }
  auto Factor(const asc::ReferenceLapackProvider& provider,
              const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace,
              asc::LapackReport& report) {
    return asc::Getc2(provider, Matrix(), Vector(rows, n), Vector(columns, n),
                      plan, workspace, report);
  }
  [[nodiscard]] auto View(const asc::LapackReport& report) const {
    return asc::ReferenceCompletePivotLuFactorView<T>::Create(
        Matrix(), Vector(rows, n), Vector(columns, n), report);
  }
  void Guards(TestContext& test) const {
    std::array<bool, 160> active{};
    for (asc::extent_t j = 0; j < n; ++j) {
      for (asc::extent_t i = 0; i < n; ++i) {
        active[Index(i, j)] = true;
      }
    }
    for (std::size_t i = 0; i < dense.size(); ++i) {
      if (!active[i]) {
        ASC_DENSE_TEST_EQ(test, dense[i], Value<T>(-83, 23));
      }
    }
    for (std::size_t i = 0; i < rhs.size(); ++i) {
      if (i == 0 || i > static_cast<std::size_t>(n)) {
        ASC_DENSE_TEST_EQ(test, rhs[i], Value<T>(-89, 29));
        ASC_DENSE_TEST_EQ(test, rows[i], -97);
        ASC_DENSE_TEST_EQ(test, columns[i], -101);
      }
    }
    ASC_DENSE_TEST_EQ(test, scale.front(), Real{-71});
    ASC_DENSE_TEST_EQ(test, scale.back(), Real{-79});
  }
};

template <typename T>
struct Scratch {
  std::array<T, 164> scalar;
  alignas(std::max_align_t) std::array<std::byte, 192> integer;
  std::size_t scalar_entries = 0;
  std::size_t integer_bytes = 0;
  Scratch() {
    scalar.fill(Value<T>(-103, 31));
    integer.fill(std::byte{0x5a});
  }
  auto Workspace(const asc::LapackWorkspacePlan& plan) {
    asc::LapackWorkspace workspace;
    scalar_entries =
        static_cast<std::size_t>(plan.regions[kScratch].minimum_entries);
    integer_bytes =
        static_cast<std::size_t>(plan.regions[kInteger].minimum_entries) *
        plan.regions[kInteger].entry_bytes;
    if (scalar_entries != 0) {
      workspace.regions[kScratch] = {scalar.data() + 1,
                                     scalar_entries * sizeof(T), kHost};
    }
    if (integer_bytes != 0) {
      workspace.regions[kInteger] = {integer.data() + 16, integer_bytes, kHost};
    }
    return workspace;
  }
  void Guards(TestContext& test) const {
    for (std::size_t i = 0; i < scalar.size(); ++i) {
      if (i == 0 || i > scalar_entries) {
        ASC_DENSE_TEST_EQ(test, scalar[i], Value<T>(-103, 31));
      }
    }
    for (std::size_t i = 0; i < integer.size(); ++i) {
      if (i < 16 || i >= 16 + integer_bytes) {
        ASC_DENSE_TEST_EQ(test, integer[i], std::byte{0x5a});
      }
    }
  }
};

template <typename T>
lapack_int NativeFactor(lapack_int n, T* a, lapack_int* rows,
                        lapack_int* columns) {
  const lapack_int leading = std::max(lapack_int{1}, n);
  lapack_int info = std::numeric_limits<lapack_int>::min();
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_sgetc2(&n, a, &leading, rows, columns, &info);
  } else if constexpr (std::is_same_v<T, double>) {
    ASC_LAPACK_dgetc2(&n, a, &leading, rows, columns, &info);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    ASC_LAPACK_cgetc2(&n, a, &leading, rows, columns, &info);
  } else {
    ASC_LAPACK_zgetc2(&n, a, &leading, rows, columns, &info);
  }
  return info;
}

template <typename T>
auto NativeSolve(lapack_int n, const T* a, T* rhs, const lapack_int* rows,
                 const lapack_int* columns) {
  auto scale = std::numeric_limits<asc::DenseBlasRealType<T>>::quiet_NaN();
  if constexpr (std::is_same_v<T, float>) {
    ASC_LAPACK_sgesc2(&n, a, &n, rhs, rows, columns, &scale);
  } else if constexpr (std::is_same_v<T, double>) {
    ASC_LAPACK_dgesc2(&n, a, &n, rhs, rows, columns, &scale);
  } else if constexpr (std::is_same_v<T, std::complex<float>>) {
    ASC_LAPACK_cgesc2(&n, a, &n, rhs, rows, columns, &scale);
  } else {
    ASC_LAPACK_zgesc2(&n, a, &n, rhs, rows, columns, &scale);
  }
  return scale;
}

}  // namespace asc_complete_pivoting_test
#endif  // ASC_TESTS_DENSE_LAPACK_COMPLETE_PIVOTING_TEST_SUPPORT_H_
