#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band_equilibration.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"

namespace {
namespace support = asc_lu_band_expert_test;
using support::Take;
using support::TestContext;

template <typename T>
long double Magnitude(T value) {
  const auto wide = support::ToWide(value);
  return std::abs(wide.real()) + std::abs(wide.imag());
}

template <typename Real>
void Near(TestContext& test, Real actual, long double expected) {
  const long double value = actual;
  const auto scale = std::max(std::abs(value), std::abs(expected));
  ASC_DENSE_TEST_CHECK(test, std::isfinite(value));
  ASC_DENSE_TEST_CHECK(test,
                       std::abs(value - expected) <=
                           64 * std::numeric_limits<Real>::epsilon() * scale);
}

template <typename T>
asc::Status Execute(
    TestContext& test, const asc::ReferenceLapackProvider& provider,
    const support::Compact<T>& matrix,
    support::Vector<asc::DenseBlasRealType<T>>& rows,
    support::Vector<asc::DenseBlasRealType<T>>& columns,
    asc::LapackEquilibrationStatistics<asc::DenseBlasRealType<T>>& stats,
    asc::LapackReport& report) {
  const auto before = matrix.values;
  const auto view = matrix.ConstView();
  const auto row_view = rows.View();
  const auto column_view = columns.View();
  const auto plan = Take(support::WithoutAllocation(test, [&] {
    return asc::QueryGbequWorkspace(provider, view, row_view, column_view,
                                    stats);
  }));
  for (const auto& region : plan.regions) {
    ASC_DENSE_TEST_EQ(test, region.minimum_entries, 0);
    ASC_DENSE_TEST_EQ(test, region.preferred_entries, 0);
  }
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  auto status = support::WithoutAllocation(test, [&] {
    return asc::Gbequ(provider, view, row_view, column_view, stats, plan,
                      workspace, report);
  });
  ASC_DENSE_TEST_CHECK(
      test, report.called_provider && report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(matrix.values, before));
  rows.Check(test);
  columns.Check(test);
  scratch.Check(test);
  return status;
}

template <typename T>
void One(TestContext& test, const asc::ReferenceLapackProvider& provider,
         const support::Compact<T>& matrix) {
  using Real = asc::DenseBlasRealType<T>;
  support::Vector<Real> rows(matrix.m);
  support::Vector<Real> columns(matrix.n);
  asc::LapackEquilibrationStatistics<Real> stats{Real{-401}, Real{-409},
                                                 Real{-419}};
  const auto row_before = rows.values;
  const auto column_before = columns.values;
  asc::LapackReport report;
  const auto status =
      Execute(test, provider, matrix, rows, columns, stats, report);
  if (matrix.m == 0 || matrix.n == 0) {
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
    ASC_DENSE_TEST_EQ(test, rows.values, row_before);
    ASC_DENSE_TEST_EQ(test, columns.values, column_before);
    ASC_DENSE_TEST_EQ(test, stats.row_condition, Real{1});
    ASC_DENSE_TEST_EQ(test, stats.column_condition, Real{1});
    ASC_DENSE_TEST_EQ(test, stats.absolute_maximum, Real{0});
    return;
  }
  std::vector<long double> row_max(static_cast<std::size_t>(matrix.m));
  for (asc::extent_t i = 0; i < matrix.m; ++i) {
    for (asc::extent_t j = 0; j < matrix.n; ++j) {
      auto& value = row_max[static_cast<std::size_t>(i)];
      value = std::max(value, Magnitude(matrix.At(i, j)));
    }
  }
  Near(test, stats.absolute_maximum,
       *std::max_element(row_max.begin(), row_max.end()));
  const auto zero_row = std::find(row_max.begin(), row_max.end(), 0);
  if (zero_row != row_max.end()) {
    const auto index = zero_row - row_max.begin();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), index + 1);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), index);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kDocumentedPartial);
    ASC_DENSE_TEST_EQ(test, columns.values, column_before);
    ASC_DENSE_TEST_EQ(test, stats.row_condition, Real{-401});
    ASC_DENSE_TEST_EQ(test, stats.column_condition, Real{-409});
    return;
  }
  const long double safe_min = std::numeric_limits<Real>::min();
  std::vector<long double> row_scale(row_max.size());
  for (std::size_t i = 0; i < row_scale.size(); ++i) {
    row_scale[i] = 1 / std::clamp(row_max[i], safe_min, 1 / safe_min);
    Near(test, rows.values[i + 1], row_scale[i]);
  }
  Near(test, stats.row_condition,
       *std::min_element(row_scale.begin(), row_scale.end()) /
           *std::max_element(row_scale.begin(), row_scale.end()));
  std::vector<long double> column_max(static_cast<std::size_t>(matrix.n));
  for (asc::extent_t j = 0; j < matrix.n; ++j) {
    for (asc::extent_t i = 0; i < matrix.m; ++i) {
      auto& value = column_max[static_cast<std::size_t>(j)];
      value = std::max(value, Magnitude(matrix.At(i, j)) *
                                  row_scale[static_cast<std::size_t>(i)]);
    }
  }
  const auto zero_column = std::find(column_max.begin(), column_max.end(), 0);
  if (zero_column != column_max.end()) {
    const auto index = zero_column - column_max.begin();
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0),
                      matrix.m + index + 1);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index.value_or(-1), index);
    ASC_DENSE_TEST_EQ(test, stats.column_condition, Real{-409});
    return;
  }
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  std::vector<long double> column_scale(column_max.size());
  for (std::size_t j = 0; j < column_scale.size(); ++j) {
    column_scale[j] = 1 / std::clamp(column_max[j], safe_min, 1 / safe_min);
    Near(test, columns.values[j + 1], column_scale[j]);
  }
  Near(test, stats.column_condition,
       *std::min_element(column_scale.begin(), column_scale.end()) /
           *std::max_element(column_scale.begin(), column_scale.end()));
}

template <typename T>
void Regular(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const int exponent = sizeof(Real) == sizeof(float) ? 100 : 800;
  std::size_t count = 0;
  for (const auto shape :
       {std::array{0, 0, 0, 0}, std::array{0, 5, 3, 2}, std::array{5, 0, 2, 3},
        std::array{1, 1, 0, 0}, std::array{1, 1, 8, 7}, std::array{3, 5, 2, 3},
        std::array{5, 3, 3, 2}, std::array{7, 7, 0, 0},
        std::array{6, 6, 2, 1}}) {
    for (const int scale : {0, -exponent, exponent}) {
      asc_lu_band_test::Band<T> band(shape[0], shape[1], shape[2], shape[3],
                                     scale);
      support::Compact<T> compact(band);
      One(test, provider, compact);
      ++count;
    }
  }
  asc_lu_band_test::Band<T> source(4, 4, 3, 3);
  support::Compact<T> zero_row(source);
  for (int j = 0; j < 4; ++j) {
    zero_row.At(2, j) = T{};
  }
  One(test, provider, zero_row);
  support::Compact<T> zero_column(source);
  for (int i = 0; i < 4; ++i) {
    zero_column.At(i, 3) = T{};
  }
  One(test, provider, zero_column);
  std::printf(
      "%zu independent compact-band equilibration profiles plus two exact-zero "
      "partial-output cases\n",
      count);
}

template <typename T>
void Extreme(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  asc_lu_band_test::Band<T> band(1, 1, 4, 6);
  support::Compact<T> compact(band);
  for (const Real value : {Real{1}, std::numeric_limits<Real>::min(),
                           std::numeric_limits<Real>::min() / Real{8},
                           std::numeric_limits<Real>::max() / Real{8}}) {
    compact.At(0, 0) = support::Value<T>(value);
    // Independently, every nonzero singleton has min(R)/max(R)=1 and
    // min(C)/max(C)=1, including when the native scale factors are clamped.
    One(test, provider, compact);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "regular" && mode != "extreme") {
    return 2;
  }
  const auto run = [&]<typename T>() {
    if (mode == "regular") {
      Regular<T>(test, provider);
    } else {
      Extreme<T>(test, provider);
    }
  };
  if (scalar == "s") {
    run.template operator()<float>();
  } else if (scalar == "d") {
    run.template operator()<double>();
  } else if (scalar == "c") {
    run.template operator()<std::complex<float>>();
  } else if (scalar == "z") {
    run.template operator()<std::complex<double>>();
  } else {
    return 2;
  }
  return test.Finish();
}
