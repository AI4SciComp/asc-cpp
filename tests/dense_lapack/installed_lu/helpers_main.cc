#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

#include "asc/core/execution.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "asc/dense/providers/lapack_lu_helpers.h"
#include "factorization_support.h"

namespace {
using installed_internal::ConstVector;
using installed_internal::kColumn;
using installed_internal::kHost;
using installed_internal::kRow;
using installed_internal::Matrix;
using installed_internal::Near;
using installed_internal::Scratch;
using installed_internal::Take;
using installed_internal::Value;
using installed_internal::Widen;

bool CalledWithoutInfo(const asc::LapackReport& report) {
  return report.called_provider && !report.native_info.has_value() &&
         report.outcome == asc::LapackOutcome::kSuccess &&
         report.output_validity == asc::LapackOutputValidity::kComplete;
}

template <typename T>
bool Swaps(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasLayout layout, asc::index_t increment) {
  Matrix<T, 3, 2> a{{}, layout};
  a.data.fill(Value<T>(-71));
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      a.At(row, column) =
          Value<T>(1 + 2 * row + column, 0.25 * static_cast<double>(row));
    }
  }
  const auto original = a;
  // Row 0 swaps with row 2; row 1 then swaps with row 2. Reversing
  // application order changes the permutation, not each pivot association.
  std::array<asc::index_t, 2> values{3, 3};
  const auto pivots = Take(asc::RawLapackPivotView::Create(
      values.data(), values.size(), asc::LapackFactorFamily::kLuPartialPivot,
      {values.data(), sizeof(values), kHost}));
  const auto plan =
      asc::QueryLaswpWorkspace(provider, a.View(), 0, 2, pivots, increment);
  Scratch<T> scratch;
  asc::LapackReport report;
  if (!plan.ok() || a.data != original.data ||
      !asc::Laswp(provider, a.View(), 0, 2, pivots, increment, *plan,
                  scratch.workspace, report)
           .ok() ||
      !CalledWithoutInfo(report) || !a.PaddingEquals(original.data) ||
      values != std::array<asc::index_t, 2>{3, 3}) {
    return false;
  }
  const auto expected_rows = increment > 0
                                 ? std::array<std::size_t, 3>{2, 0, 1}
                                 : std::array<std::size_t, 3>{1, 2, 0};
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      if (a.At(row, column) != original.At(expected_rows[row], column)) {
        return false;
      }
    }
  }
  return true;
}

template <typename T>
bool Scaling(const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasLayout layout, int mode) {
  using Real = asc::DenseBlasRealType<T>;
  Matrix<T, 2, 2> a{{}, layout};
  a.data.fill(Value<T>(-61));
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      a.At(row, column) = Value<T>(
          1 + 2 * row + column, 0.25 * static_cast<double>(1 + row + column));
    }
  }
  const auto original = a;
  std::array<Real, 2> rows{Real{0.5}, Real{0.25}};
  std::array<Real, 2> columns{Real{2}, Real{4}};
  asc::LapackEquilibrationStatistics<Real> statistics;
  const bool scale_rows = (mode & 1) != 0;
  const bool scale_columns = (mode & 2) != 0;
  statistics.row_condition = scale_rows ? Real{0.0625} : Real{1};
  statistics.column_condition = scale_columns ? Real{0.0625} : Real{1};
  statistics.absolute_maximum = Real{4};
  auto applied = asc::LapackEquilibration::kBoth;
  const auto plan =
      asc::QueryLaqgeWorkspace(provider, a.View(), ConstVector(rows),
                               ConstVector(columns), statistics, applied);
  Scratch<T> scratch;
  asc::LapackReport report;
  if (!plan.ok() || a.data != original.data ||
      !asc::Laqge(provider, a.View(), ConstVector(rows), ConstVector(columns),
                  statistics, applied, *plan, scratch.workspace, report)
           .ok() ||
      !CalledWithoutInfo(report) || !a.PaddingEquals(original.data)) {
    return false;
  }
  const std::array expected_modes{
      asc::LapackEquilibration::kNone, asc::LapackEquilibration::kRows,
      asc::LapackEquilibration::kColumns, asc::LapackEquilibration::kBoth};
  if (applied != expected_modes[static_cast<std::size_t>(mode)]) {
    return false;
  }
  for (std::size_t row = 0; row < 2; ++row) {
    for (std::size_t column = 0; column < 2; ++column) {
      const auto expected =
          Widen(original.At(row, column)) *
          static_cast<long double>(scale_rows ? rows[row] : 1) *
          static_cast<long double>(scale_columns ? columns[column] : 1);
      if (!Near<T>(Widen(a.At(row, column)), expected, 4)) {
        return false;
      }
    }
  }
  return rows == std::array<Real, 2>{Real{0.5}, Real{0.25}} &&
         columns == std::array<Real, 2>{Real{2}, Real{4}};
}

template <typename T>
bool AllModes(const asc::ReferenceLapackProvider& provider) {
  for (const auto layout : {kRow, kColumn}) {
    for (const asc::index_t increment : {1, -1}) {
      if (!Swaps<T>(provider, layout, increment)) {
        return false;
      }
    }
    for (int mode = 0; mode < 4; ++mode) {
      if (!Scaling<T>(provider, layout, mode)) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

int main() {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const bool passed = AllModes<float>(provider) && AllModes<double>(provider) &&
                      AllModes<std::complex<float>>(provider) &&
                      AllModes<std::complex<double>>(provider);
  std::puts(passed ? "Installed LU helpers: all eight scalar routes passed."
                   : "Installed LU helpers failed.");
  return passed ? 0 : 1;
}
