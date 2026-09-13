#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_EXPERT_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_EXPERT_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/workspace.h"
#include "indefinite_test_support.h"

namespace asc_indefinite_expert_test {
using asc_indefinite_test::Adjoint;
using asc_indefinite_test::EqualBytes;
using asc_indefinite_test::kColumn;
using asc_indefinite_test::kHost;
using asc_indefinite_test::kUpper;
using asc_indefinite_test::Take;
using asc_indefinite_test::TestContext;
using asc_indefinite_test::ToWide;
using asc_indefinite_test::Value;
using asc_indefinite_test::Wide;

inline std::size_t Offset(int i, int j, asc::DenseBlasLayout layout, int ld) {
  return 1U +
         static_cast<std::size_t>(layout == kColumn ? j * ld + i : i * ld + j);
}

inline long double Abs1(Wide value) {
  return std::abs(value.real()) + std::abs(value.imag());
}

template <typename T, std::size_t Size>
auto Vector(std::array<T, Size>& data, int n) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      data.data() + 1, n, 1, {data.data(), sizeof(data), kHost}));
}

template <typename T>
struct Scratch : asc_indefinite_test::Scratch<T> {
  std::array<asc::DenseBlasRealType<T>, 80> real{};
  Scratch() { real.fill(-211); }

  asc::LapackWorkspace Workspace(const asc::LapackWorkspacePlan& plan,
                                 asc::extent_t scalar_entries = -1) {
    auto workspace =
        asc_indefinite_test::Scratch<T>::Workspace(plan, scalar_entries);
    constexpr auto kRole =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
    const auto bytes =
        plan.regions[kRole].preferred_entries * plan.regions[kRole].entry_bytes;
    if (bytes != 0) {
      if (bytes > (real.size() - 2) * sizeof(real[0])) {
        std::abort();
      }
      workspace.regions[kRole] = {real.data() + 1, bytes, kHost};
    }
    return workspace;
  }

  void Guards(TestContext& test, const asc::LapackWorkspace& workspace) const {
    asc_indefinite_test::Scratch<T>::Guards(test, workspace);
    constexpr auto kRole =
        static_cast<std::size_t>(asc::LapackWorkspaceKind::kReal);
    ASC_DENSE_TEST_EQ(test, real.front(), -211);
    for (std::size_t i = 1 + workspace.regions[kRole].size() / sizeof(real[0]);
         i < real.size(); ++i) {
      ASC_DENSE_TEST_EQ(test, real[i], -211);
    }
  }
};

// An independently constructed permuted congruence of known 1-by-1 and
// 2-by-2 blocks. Both the matrix and right-hand side are rounded once to the
// public scalar type; numerical oracles use those actual rounded operands.
template <typename T>
struct Problem {
  using Real = asc::DenseBlasRealType<T>;
  int n;
  int nrhs;
  bool hermitian;
  std::array<Wide, 49> full{};
  std::array<T, 24> rhs{};
  std::array<T, 24> expected{};

  Problem(int order, int count, bool he, int exponent)
      : n(order), nrhs(count), hermitian(he) {
    std::array<Wide, 49> lower{};
    std::array<Wide, 49> diagonal{};
    const Real scale = std::ldexp(Real{1}, exponent);
    for (int i = 0; i < n; ++i) {
      lower[i * n + i] = 1;
      for (int j = 0; j < i; ++j) {
        lower[i * n + j] = ToWide(
            Value<T>(static_cast<long double>((i + j) % 3 - 1) / 32,
                     static_cast<long double>((i + 2 * j) % 3 - 1) / 64));
      }
      if (i >= 2 || n == 1) {
        diagonal[i * n + i] =
            ToWide(Value<T>(i % 2 == 0 ? -3 : 5, hermitian ? 0 : 0.25L));
      }
    }
    if (n >= 2) {
      diagonal[1] = ToWide(Value<T>(2, 0.5L));
      diagonal[n] = Adjoint(diagonal[1], hermitian);
    }
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j <= i; ++j) {
        Wide sum{};
        for (int p = 0; p < n; ++p) {
          for (int q = 0; q < n; ++q) {
            sum += lower[i * n + p] * diagonal[p * n + q] *
                   Adjoint(lower[j * n + q], hermitian);
          }
        }
        if (i == j && hermitian) {
          sum.imag(0);
        }
        const auto rounded = Value<T>(sum.real(), sum.imag()) * scale;
        const int row = (i + n / 2) % n;
        const int column = (j + n / 2) % n;
        full[row * n + column] = ToWide(rounded);
        full[column * n + row] = Adjoint(ToWide(rounded), hermitian);
      }
    }
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        expected[j * n + i] =
            Value<T>(static_cast<long double>((i + j) % 3 - 1),
                     static_cast<long double>((i + 2 * j) % 5 - 2) / 4);
      }
      for (int i = 0; i < n; ++i) {
        Wide sum{};
        for (int k = 0; k < n; ++k) {
          sum += full[i * n + k] * ToWide(expected[j * n + k]);
        }
        rhs[j * n + i] = Value<T>(sum.real(), sum.imag());
      }
    }
  }

  template <std::size_t Size>
  void Original(std::array<T, Size>& a, asc::DenseBlasTriangle triangle,
                asc::DenseBlasLayout layout, int ld) const {
    a.fill(Value<T>(-223, 17));
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        const bool selected = triangle == kUpper ? i <= j : i >= j;
        const auto value = full[i * n + j];
        a[Offset(i, j, layout, ld)] =
            selected ? Value<T>(value.real(), value.imag())
                     : Value<T>(std::numeric_limits<Real>::quiet_NaN());
        if constexpr (asc::DenseBlasComplex<T>) {
          if (hermitian && i == j) {
            a[Offset(i, j, layout, ld)].imag(
                std::numeric_limits<Real>::quiet_NaN());
          }
        }
      }
    }
  }

  template <std::size_t Size>
  void Rhs(std::array<T, Size>& b, asc::DenseBlasLayout layout, int ld) const {
    b.fill(Value<T>(-227, 19));
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        b[Offset(i, j, layout, ld)] = rhs[j * n + i];
      }
    }
  }

  // Independent componentwise residual denominator. Real and imaginary
  // components use LAPACK's ABS1 convention, not std::abs(complex).
  template <std::size_t Size>
  [[nodiscard]] long double Backward(const std::array<T, Size>& x,
                                     asc::DenseBlasLayout layout, int ld,
                                     int column,
                                     bool source_safe_terms = true) const {
    const long double safe1 =
        static_cast<long double>(n + 1) * std::numeric_limits<Real>::min();
    const long double safe2 =
        safe1 / (std::numeric_limits<Real>::epsilon() / 2);
    long double error = 0;
    for (int i = 0; i < n; ++i) {
      Wide residual = ToWide(rhs[column * n + i]);
      long double denominator = Abs1(residual);
      for (int k = 0; k < n; ++k) {
        const auto entry = ToWide(x[Offset(k, column, layout, ld)]);
        residual -= full[i * n + k] * entry;
        denominator += Abs1(full[i * n + k]) * Abs1(entry);
      }
      const long double guarded =
          source_safe_terms && denominator <= safe2 ? safe1 : 0;
      if (denominator == 0 && guarded == 0) {
        if (residual != Wide{}) {
          return std::numeric_limits<long double>::infinity();
        }
        continue;
      }
      error =
          std::max(error, (Abs1(residual) + guarded) / (denominator + guarded));
    }
    return error;
  }
};

}  // namespace asc_indefinite_expert_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_EXPERT_TEST_SUPPORT_H_
