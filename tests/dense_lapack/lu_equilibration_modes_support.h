#ifndef ASC_TESTS_DENSE_LAPACK_LU_EQUILIBRATION_MODES_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_LU_EQUILIBRATION_MODES_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "lu_aux_info_test_support.h"

// Reuse ASC's existing fixture and independent binary-decomposition oracle
// unchanged. The original ordinary/fidelity test remains separately compiled.
namespace asc_equilibration_modes_test {
using asc_lu_aux_info_test::Take;
using asc_lu_aux_info_test::TestContext;
constexpr auto kHost = asc::MemorySpace::kHost;
constexpr auto kColumn = asc::DenseBlasLayout::kColumnMajor;
constexpr auto kRow = asc::DenseBlasLayout::kRowMajor;
constexpr auto kLayout =
    static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);

template <typename T>
T Value(double real, double imaginary = 0) {
  using Real = asc::DenseBlasRealType<T>;
  if constexpr (asc::DenseBlasComplex<T>) {
    return T{static_cast<Real>(real), static_cast<Real>(imaginary)};
  } else {
    return static_cast<T>(real);
  }
}

template <typename T>
asc::DenseBlasRealType<T> Magnitude(T value) {
  if constexpr (asc::DenseBlasComplex<T>) {
    return std::abs(value.real()) + std::abs(value.imag());
  } else {
    return std::abs(value);
  }
}

template <typename Real>
Real Quantize(Real value) {
  // Independent binary decomposition, not the provider's LOG/RADIX**INT path.
  int exponent = 0;
  const Real fraction = std::frexp(value, &exponent);
  --exponent;
  if (value < Real{1} && fraction != Real{0.5}) {
    ++exponent;  // INT(log2(x)) truncates toward zero, including x<1.
  }
  return std::ldexp(Real{1}, exponent);
}

template <typename T, std::size_t Size>
auto Vector(std::array<T, Size>& values, asc::extent_t count,
            std::size_t offset = 1, asc::stride_t increment = 1) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      values.data() + offset, count, increment,
      {values.data(), values.size() * sizeof(T), kHost}));
}

template <typename T>
struct Sample {
  using Real = asc::DenseBlasRealType<T>;
  asc::DenseBlasLayout layout;
  std::array<T, 24> matrix;
  std::array<T, 12> scratch;
  std::array<Real, 5> rows;
  std::array<Real, 4> columns;
  asc::LapackEquilibrationStatistics<Real> statistics{-31, -37, -41};

  Sample(asc::DenseBlasLayout order, int exponent) : layout(order) {
    matrix.fill(Value<T>(-709, 29));
    scratch.fill(Value<T>(-719, 31));
    rows.fill(Real{-53});
    columns.fill(Real{-59});
    const Real scale = std::ldexp(Real{1}, exponent);
    At(0, 0) = scale * Value<T>(3, 4);
    At(1, 0) = scale * Value<T>(5, -2);
    At(2, 0) = scale * Value<T>(12, 1);
    At(0, 1) = scale * Value<T>(0.5, 0.25);
    At(1, 1) = scale * Value<T>(0.25, 0.25);
    At(2, 1) = scale * Value<T>(1, -0.5);
  }
  T& At(int i, int j) {
    return matrix[layout == kColumn ? 5 * j + i : 4 * i + j];
  }
  [[nodiscard]] auto Matrix() const {
    return Take(asc::DenseBlasMatrixView<const T>::Create(
        matrix.data(), 3, 2, layout, layout == kColumn ? 5 : 4,
        {matrix.data(), sizeof(matrix), kHost}));
  }
  auto Workspace() {
    asc::LapackWorkspace result;
    if (layout == kRow) {
      result.regions[kLayout] = {scratch.data() + 1, 6 * sizeof(T), kHost};
    }
    return result;
  }
  auto Query(const asc::ReferenceLapackProvider& provider, bool radix) {
    return radix
               ? asc::QueryGeequbWorkspace(provider, Matrix(), Vector(rows, 3),
                                           Vector(columns, 2), statistics)
               : asc::QueryGeequWorkspace(provider, Matrix(), Vector(rows, 3),
                                          Vector(columns, 2), statistics);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider, bool radix,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    return radix ? asc::Geequb(provider, Matrix(), Vector(rows, 3),
                               Vector(columns, 2), statistics, plan, workspace,
                               report)
                 : asc::Geequ(provider, Matrix(), Vector(rows, 3),
                              Vector(columns, 2), statistics, plan, workspace,
                              report);
  }
};

template <typename Real>
void Near(TestContext& test, Real actual, Real expected) {
  if (std::abs(actual - expected) >
      16 * std::numeric_limits<Real>::epsilon() *
          std::max(std::abs(expected), std::numeric_limits<Real>::min())) {
    std::fprintf(stderr, "Scale diagnostic: actual=%a expected=%a\n",
                 static_cast<double>(actual), static_cast<double>(expected));
  }
  ASC_DENSE_TEST_CHECK(
      test,
      std::abs(actual - expected) <=
          16 * std::numeric_limits<Real>::epsilon() *
              std::max(std::abs(expected), std::numeric_limits<Real>::min()));
}

template <typename T>
void ExpectedScales(TestContext& test, Sample<T>& sample, bool radix) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 3> row_max{};
  std::array<Real, 3> expected_rows{};
  std::array<Real, 2> column_max{};
  const Real safe_minimum = std::numeric_limits<Real>::min();
  const Real safe_maximum = Real{1} / safe_minimum;
  for (int i = 0; i < 3; ++i) {
    row_max[i] =
        std::max(Magnitude(sample.At(i, 0)), Magnitude(sample.At(i, 1)));
    if (radix) {
      row_max[i] = Quantize(row_max[i]);
    }
    expected_rows[i] =
        Real{1} / std::clamp(row_max[i], safe_minimum, safe_maximum);
    Near(test, sample.rows[i + 1], expected_rows[i]);
  }
  for (int j = 0; j < 2; ++j) {
    for (int i = 0; i < 3; ++i) {
      column_max[j] = std::max(column_max[j],
                               Magnitude(sample.At(i, j)) * expected_rows[i]);
    }
    if (radix) {
      column_max[j] = Quantize(column_max[j]);
    }
    Near(test, sample.columns[j + 1],
         Real{1} / std::clamp(column_max[j], safe_minimum, safe_maximum));
  }
  const auto [minimum_row, maximum_row] =
      std::minmax_element(row_max.begin(), row_max.end());
  const auto [minimum_column, maximum_column] =
      std::minmax_element(column_max.begin(), column_max.end());
  Near(test, sample.statistics.row_condition,
       std::clamp(*minimum_row, safe_minimum, safe_maximum) /
           std::min(*maximum_row, safe_maximum));
  Near(test, sample.statistics.column_condition,
       std::clamp(*minimum_column, safe_minimum, safe_maximum) /
           std::min(*maximum_column, safe_maximum));
  Near(test, sample.statistics.absolute_maximum, *maximum_row);
}

}  // namespace asc_equilibration_modes_test
#endif  // ASC_TESTS_DENSE_LAPACK_LU_EQUILIBRATION_MODES_SUPPORT_H_
