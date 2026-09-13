#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed_solve.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_packed_test_support.h"
#include "indefinite_test_support.h"
#include "tests/dense/test_support.h"

namespace asc_packed_solve_test {
namespace factor = asc_packed_indefinite_test;
namespace base = asc_indefinite_test;
using base::EqualBytes;
using base::kColumn;
using base::kHost;
using base::kLower;
using base::kRow;
using base::kUpper;
using base::Take;
using base::TestContext;
using base::ToWide;
using base::Value;
using base::Wide;

template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasPackedMatrixView<const T> factors,
           asc::RawLapackPivotView pivots, asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHptrsWorkspace(provider, triangle, factors, pivots, rhs);
    }
  }
  return asc::QuerySptrsWorkspace(provider, triangle, factors, pivots, rhs);
}

template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, bool hermitian,
                  asc::DenseBlasPackedMatrixView<const T> factors,
                  asc::RawLapackPivotView pivots,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hptrs(provider, triangle, factors, pivots, rhs, plan,
                        workspace, report);
    }
  }
  return asc::Sptrs(provider, triangle, factors, pivots, rhs, plan, workspace,
                    report);
}

template <typename T>
struct Fixture {
  factor::Sample<T> a;
  int nrhs;
  asc::DenseBlasLayout rhs_layout;
  std::array<T, 700> rhs;
  std::array<T, 700> original_rhs;
  std::array<T, 5000> original_factors;
  std::array<asc::index_t, 72> original_pivots;

  Fixture(int n, int columns, bool hermitian, asc::DenseBlasTriangle triangle,
          asc::DenseBlasLayout factor_layout, asc::DenseBlasLayout b_layout,
          int exponent, int kind)
      : a(n, hermitian, triangle, factor_layout, exponent),
        nrhs(columns),
        rhs_layout(b_layout) {
    a.Reset(kind);
    rhs.fill(Value<T>(-631, 47));
    original_rhs = rhs;
    original_factors = a.a;
    original_pivots = a.pivots;
  }

  [[nodiscard]] int Leading() const {
    return rhs_layout == kColumn ? a.n + 1 : nrhs + 1;
  }
  [[nodiscard]] std::size_t Offset(int i, int j) const {
    return 1 + static_cast<std::size_t>(rhs_layout == kColumn
                                            ? j * Leading() + i
                                            : i * Leading() + j);
  }
  [[nodiscard]] T Solution(int i, int j) const {
    return Value<T>(static_cast<long double>((i + j) % 3 + 1) / 4,
                    static_cast<long double>((2 * i + j) % 3 - 1) / 8);
  }
  auto Rhs() {
    return Take(asc::DenseBlasMatrixView<T>::Create(
        rhs.data() + 1, a.n, nrhs, rhs_layout, Leading(),
        {rhs.data(), sizeof(rhs), kHost}));
  }
  [[nodiscard]] auto Pivots() const {
    return Take(asc::RawLapackPivotView::Create(
        a.pivots.data() + 1, a.n, asc::LapackFactorFamily::kBunchKaufman,
        {a.pivots.data(), sizeof(a.pivots), kHost}));
  }
  void FillRhs() {
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < a.n; ++i) {
        Wide value{};
        for (int k = 0; k < a.n; ++k) {
          value += a.full[i * a.n + k] * ToWide(Solution(k, j));
        }
        rhs[Offset(i, j)] = Value<T>(value.real(), value.imag());
      }
    }
    original_rhs = rhs;
  }
  void Prepare(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
    const auto plan =
        Take(factor::Query(provider, a.triangle, a.hermitian, a.View(),
                           base::Pivots(a.pivots, a.n)));
    base::Scratch<T> scratch;
    const auto work = scratch.Workspace(plan);
    asc::LapackReport report;
    const auto status =
        factor::Factor(provider, a.triangle, a.hermitian, a.View(),
                       base::Pivots(a.pivots, a.n), plan, work, report);
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    a.Reconstruction(test);
    scratch.Guards(test, work);
    original_factors = a.a;
    original_pivots = a.pivots;
    FillRhs();
  }
  void UnchangedInputs(TestContext& test) const {
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(a.a.data(), original_factors.data(), sizeof(a.a)));
    ASC_DENSE_TEST_EQ(test, a.pivots, original_pivots);
  }
  void Padding(TestContext& test) const {
    auto expected = original_rhs;
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < a.n; ++i) {
        expected[Offset(i, j)] = rhs[Offset(i, j)];
      }
    }
    ASC_DENSE_TEST_CHECK(test,
                         EqualBytes(rhs.data(), expected.data(), sizeof(rhs)));
  }
  void Mathematics(TestContext& test) const {
    long double norm = 0;
    long double error = 0;
    long double residual = 0;
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < a.n; ++i) {
        const auto difference =
            std::abs(ToWide(rhs[Offset(i, j)]) - ToWide(Solution(i, j)));
        ASC_DENSE_TEST_CHECK(test, std::isfinite(difference));
        error = std::max(error, difference);
        Wide product{};
        long double row_norm = 0;
        for (int k = 0; k < a.n; ++k) {
          product += a.full[i * a.n + k] * ToWide(rhs[Offset(k, j)]);
          row_norm += std::abs(a.full[i * a.n + k]);
        }
        const auto difference_rhs =
            std::abs(product - ToWide(original_rhs[Offset(i, j)]));
        ASC_DENSE_TEST_CHECK(test, std::isfinite(difference_rhs));
        residual = std::max(residual, difference_rhs);
        norm = std::max(norm, row_norm);
      }
    }
    // These small, ordinary dyadic fixtures have known bounded solutions.
    // Check both forward error and the independently accumulated equation.
    const auto bound =
        256 * std::max(a.n, 1) *
        std::numeric_limits<asc::DenseBlasRealType<T>>::epsilon();
    ASC_DENSE_TEST_CHECK(test, error <= bound);
    ASC_DENSE_TEST_CHECK(test, residual <= bound * norm);
  }
};
}  // namespace asc_packed_solve_test

#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_SOLVE_TEST_SUPPORT_H_
