#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <type_traits>

#include "../../src/dense/lapack/internal_lu_helpers.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "asc/dense/providers/lapack_lu_helpers.h"
#include "lu_helpers_failure_test.h"
#include "lu_helpers_scaling_test.h"
#include "lu_helpers_test_support.h"

namespace {
using asc_helpers_test::kColumn;
using asc_helpers_test::kInteger;
using asc_helpers_test::kLayout;
using asc_helpers_test::kRow;
using asc_helpers_test::Padding;
using asc_helpers_test::SameBits;
using asc_helpers_test::Sample;
using asc_helpers_test::Take;
using asc_helpers_test::TestContext;
using asc_helpers_test::ToWide;
using asc_helpers_test::Unchanged;
using asc_helpers_test::WithoutAllocation;

void CheckReport(TestContext& test, const asc::LapackReport& report,
                 bool called) {
  ASC_DENSE_TEST_EQ(test, report.called_provider, called);
  ASC_DENSE_TEST_CHECK(test, !report.native_info && !report.native_argument &&
                                 !report.diagnostic_index &&
                                 !report.factor_family);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kComplete);
}

template <typename T>
void IntegerGuards(TestContext& test, const Sample<T>& sample,
                   const Sample<T>& before, asc::index_t first,
                   asc::extent_t count, asc::index_t increment,
                   std::size_t entry_bytes) {
  std::array<bool, 528> used{};
  for (asc::extent_t i = 0; i < count; ++i) {
    const auto offset = first + i * std::abs(increment);
    for (std::size_t j = 0; j < entry_bytes; ++j) {
      used[8 + static_cast<std::size_t>(offset) * entry_bytes + j] = true;
    }
  }
  for (std::size_t i = 0; i < sample.integer.size(); ++i) {
    if (!used[i]) {
      ASC_DENSE_TEST_EQ(test, sample.integer[i], before.integer[i]);
    }
  }
}

template <typename T>
void SwapCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Sample<T>& sample, asc::index_t first, asc::extent_t count,
              asc::index_t increment) {
  const auto spacing = std::abs(increment);
  for (asc::extent_t j = 0; j < count; ++j) {
    sample.pivots[1 + first + j * spacing] = (j * 5 + 2) % sample.m + 1;
  }
  const auto before = sample;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryLaswpWorkspace(provider, sample.Matrix(), first, count,
                                    sample.Pivots(), increment);
  }));
  Unchanged(test, sample, before);
  ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].minimum_entries,
                    first + 1 + (count - 1) * spacing);
  ASC_DENSE_TEST_EQ(test, plan.regions[kLayout].minimum_entries,
                    sample.layout == kRow ? sample.m * sample.n : 0);
  std::array<asc::index_t, 8> permutation{0, 1, 2, 3, 4, 5, 6, 7};
  for (asc::extent_t step = 0; step < count; ++step) {
    const auto j = increment > 0 ? step : count - 1 - step;
    const auto destination = sample.pivots[1 + first + j * spacing] - 1;
    std::swap(permutation[first + j], permutation[destination]);
  }
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Laswp(provider, sample.Matrix(),
                                                 first, count, sample.Pivots(),
                                                 increment, plan,
                                                 sample.Workspace(), report);
                             }).ok());
  CheckReport(test, report, true);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  for (asc::extent_t i = 0; i < sample.m; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(i, j)],
                        before.a[before.Offset(permutation[i], j)]);
    }
  }
  Padding(test, sample, before);
  IntegerGuards(test, sample, before, first, count, increment,
                plan.regions[kInteger].entry_bytes);
  ASC_DENSE_TEST_EQ(test, sample.pivots, before.pivots);
  const auto reverse = Take(asc::QueryLaswpWorkspace(
      provider, sample.Matrix(), first, count, sample.Pivots(), -increment));
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Laswp(provider, sample.Matrix(),
                                                 first, count, sample.Pivots(),
                                                 -increment, reverse,
                                                 sample.Workspace(), report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, sample.a, before.a);
}

template <typename T>
void SwapNumerics(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  for (auto layout : {kColumn, kRow}) {
    for (asc::extent_t m : {1, 3, 8}) {
      for (asc::extent_t n : {1, 31, 32, 33, 64, 65}) {
        for (asc::index_t first : {0, 1}) {
          if (first >= m) {
            continue;
          }
          for (asc::extent_t count : {asc::extent_t{1}, m - first}) {
            for (asc::index_t increment : {1, 2, 3, -1, -2, -3}) {
              Sample<T> sample(m, n, layout);
              SwapCase(test, provider, sample, first, count, increment);
            }
          }
        }
      }
    }
  }
}

template <typename T>
void SwapNoOps(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  for (auto layout : {kColumn, kRow}) {
    for (int mode = 0; mode < 4; ++mode) {
      Sample<T> sample(mode == 3 ? 0 : 3, mode == 2 ? 0 : 4, layout);
      const auto before = sample;
      const asc::index_t increment = mode == 0 ? 0 : -3;
      const asc::extent_t count = mode == 1 || mode == 3 ? 0 : 3;
      const asc::index_t first = mode == 1 ? 3 : 0;
      const auto plan = Take(WithoutAllocation(test, [&] {
        return asc::QueryLaswpWorkspace(provider, sample.Matrix(), first, count,
                                        sample.Pivots(0), increment);
      }));
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                   return asc::Laswp(
                                       provider, sample.Matrix(), first, count,
                                       sample.Pivots(0), increment, plan, {},
                                       report);
                                 }).ok());
      Unchanged(test, sample, before);
      CheckReport(test, report, false);
    }
  }
}

template <typename T>
void ReverseMinimumIncrement(TestContext& test,
                             const asc::ReferenceLapackProvider& provider) {
  const auto increment =
      provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
          ? static_cast<asc::index_t>(std::numeric_limits<std::int32_t>::min())
          : std::numeric_limits<asc::index_t>::min();
  for (auto layout : {kColumn, kRow}) {
    Sample<T> sample(3, 2, layout);
    sample.pivots[2] = 1;
    const auto before = sample;
    const auto plan = Take(asc::QueryLaswpWorkspace(
        provider, sample.Matrix(), 1, 1, sample.Pivots(2), increment));
    asc::LapackReport report;
    ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                 return asc::Laswp(provider, sample.Matrix(), 1,
                                                   1, sample.Pivots(2),
                                                   increment, plan,
                                                   sample.Workspace(), report);
                               }).ok());
    CheckReport(test, report, true);
    for (asc::extent_t j = 0; j < 2; ++j) {
      ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(0, j)],
                        before.a[sample.Offset(1, j)]);
      ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(1, j)],
                        before.a[sample.Offset(0, j)]);
      ASC_DENSE_TEST_EQ(test, sample.a[sample.Offset(2, j)],
                        before.a[sample.Offset(2, j)]);
    }
    Padding(test, sample, before);
  }
}

template <typename Real>
asc::LapackEquilibration ExpectedMode(
    const asc::LapackEquilibrationStatistics<Real>& statistics) {
  const long double small =
      static_cast<long double>(std::numeric_limits<Real>::min()) /
      std::numeric_limits<Real>::epsilon();
  const bool rows = statistics.row_condition < static_cast<Real>(0.1L) ||
                    statistics.absolute_maximum < small ||
                    statistics.absolute_maximum > 1 / small;
  const bool columns = statistics.column_condition < static_cast<Real>(0.1L);
  if (rows) {
    return columns ? asc::LapackEquilibration::kBoth
                   : asc::LapackEquilibration::kRows;
  }
  return columns ? asc::LapackEquilibration::kColumns
                 : asc::LapackEquilibration::kNone;
}

template <typename T>
void ScaleCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Sample<T>& sample) {
  using Real = asc::DenseBlasRealType<T>;
  const auto mode = ExpectedMode(sample.statistics);
  const bool rows = mode == asc::LapackEquilibration::kBoth ||
                    mode == asc::LapackEquilibration::kRows;
  const bool columns = mode == asc::LapackEquilibration::kBoth ||
                       mode == asc::LapackEquilibration::kColumns;
  for (asc::extent_t i = 0; i < sample.m; ++i) {
    sample.rows[i + 1] = rows ? std::ldexp(Real{1}, static_cast<int>(i) - 2)
                              : std::numeric_limits<Real>::quiet_NaN();
  }
  for (asc::extent_t j = 0; j < sample.n; ++j) {
    sample.columns[j + 1] = columns
                                ? std::ldexp(Real{1}, 1 - static_cast<int>(j))
                                : std::numeric_limits<Real>::quiet_NaN();
  }
  const auto before = sample;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return asc::QueryLaqgeWorkspace(provider, sample.Matrix(), sample.Rows(),
                                    sample.Columns(), sample.statistics,
                                    sample.applied);
  }));
  Unchanged(test, sample, before);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Laqge(
                                   provider, sample.Matrix(), sample.Rows(),
                                   sample.Columns(), sample.statistics,
                                   sample.applied, plan, sample.Workspace(),
                                   report);
                             }).ok());
  CheckReport(test, report, true);
  ASC_DENSE_TEST_EQ(test, sample.applied, mode);
  for (asc::extent_t i = 0; i < sample.m; ++i) {
    for (asc::extent_t j = 0; j < sample.n; ++j) {
      auto expected = ToWide(before.a[before.Offset(i, j)]);
      if (rows) {
        expected *= before.rows[i + 1];
      }
      if (columns) {
        expected *= before.columns[j + 1];
      }
      const auto actual = ToWide(sample.a[sample.Offset(i, j)]);
      ASC_DENSE_TEST_CHECK(test, std::abs(actual - expected) <=
                                     4 * std::numeric_limits<Real>::epsilon() *
                                         std::abs(expected));
    }
  }
  Padding(test, sample, before);
  ASC_DENSE_TEST_CHECK(test, SameBits(sample.rows, before.rows));
  ASC_DENSE_TEST_CHECK(test, SameBits(sample.columns, before.columns));
  ASC_DENSE_TEST_EQ(test, sample.integer, before.integer);
  if (mode == asc::LapackEquilibration::kNone) {
    ASC_DENSE_TEST_EQ(test, sample.packed, before.packed);
  }
}

template <typename T>
void ScaleNumerics(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  const Real threshold = static_cast<Real>(0.1L);
  const Real small =
      std::numeric_limits<Real>::min() / std::numeric_limits<Real>::epsilon();
  const Real large = 1 / small;
  for (auto layout : {kColumn, kRow}) {
    for (Real rowcnd :
         {Real{1}, threshold, std::nextafter(threshold, Real{0}), Real{0}}) {
      for (Real colcnd :
           {Real{1}, threshold, std::nextafter(threshold, Real{0}), Real{0}}) {
        for (Real amax :
             {Real{1}, small, std::nextafter(small, Real{0}), large,
              std::nextafter(large, std::numeric_limits<Real>::infinity()),
              Real{0}}) {
          Sample<T> sample(3, 4, layout);
          sample.statistics = {rowcnd, colcnd, amax};
          ScaleCase(test, provider, sample);
        }
      }
    }
    for (auto shape : {std::array{1, 1}, std::array{1, 7}, std::array{8, 1},
                       std::array{8, 7}}) {
      Sample<T> sample(shape[0], shape[1], layout);
      sample.statistics = {Real{0.01}, Real{0.01}, Real{1}};
      ScaleCase(test, provider, sample);
    }
  }
}

void IntegerBoundaries(TestContext& test) {
  using asc::internal_lu_helpers::ScaleDimensions;
  using asc::internal_lu_helpers::SwapCapacity;
  for (asc::extent_t maximum :
       {static_cast<asc::extent_t>(std::numeric_limits<std::int32_t>::max()),
        std::numeric_limits<asc::extent_t>::max()}) {
    const auto minimum = -maximum - 1;
    ASC_DENSE_TEST_EQ(test, Take(SwapCapacity(3, 2, 3, 1, 1, minimum, maximum)),
                      2);
    ASC_DENSE_TEST_EQ(
        test, SwapCapacity(3, 2, 3, 0, 2, minimum, maximum).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, SwapCapacity(3, 2, 3, 0, 1, maximum, maximum).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test, Take(SwapCapacity(3, 2, 3, 0, 1, maximum - 1, maximum)), 1);
    ASC_DENSE_TEST_EQ(
        test, Take(SwapCapacity(3, 2, 3, 0, 2, -(maximum - 1), maximum)),
        maximum);
    ASC_DENSE_TEST_EQ(
        test, SwapCapacity(3, maximum, 3, 0, 1, 1, maximum).status().code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Take(SwapCapacity(3, maximum, 3, 0, 0, 1, maximum)),
                      0);
    ASC_DENSE_TEST_EQ(
        test,
        SwapCapacity(maximum, 1, maximum, maximum - 1, 1, 1, maximum)
            .status()
            .code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(
        test,
        Take(SwapCapacity(maximum, 1, maximum, maximum - 1, 1, -1, maximum)),
        maximum);
    ASC_DENSE_TEST_CHECK(
        test, ScaleDimensions(maximum, 1, maximum, false, maximum).ok());
    ASC_DENSE_TEST_EQ(
        test, ScaleDimensions(maximum, 1, maximum, true, maximum).code(),
        asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_CHECK(test,
                         ScaleDimensions(maximum, 0, 1, true, maximum).ok());
  }
  ASC_DENSE_TEST_EQ(test, SwapCapacity(3, 1, 3, 2, 2, 1, 127).status().code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, SwapCapacity(3, 1, 3, -1, 1, 1, 127).status().code(),
                    asc::ErrorCode::kInvalidArgument);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  SwapNumerics<T>(test, provider);
  SwapNoOps<T>(test, provider);
  ReverseMinimumIncrement<T>(test, provider);
  ScaleNumerics<T>(test, provider);
  asc_helpers_test::EquilibrationPipeline<T>(test, provider);
  asc_helpers_test::UnusedAndNonfiniteScales<T>(test, provider);
  asc_helpers_test::WorkspaceFailures<T>(test, provider, false);
  asc_helpers_test::WorkspaceFailures<T>(test, provider, true);
  asc_helpers_test::IntegerWorkspaceFailures<T>(test, provider);
  asc_helpers_test::EmptySwapWideStride<T>(test, provider);
  asc_helpers_test::PivotFailures<T>(test, provider);
  asc_helpers_test::ScaleFailures<T>(test, provider);
  asc_helpers_test::EmptyScale<T>(test, provider);
}
}  // namespace

int main(int argc, char** argv) {
  TestContext test;
  if (argc != 2) {
    std::fputs("Expected one scalar selector: s, d, c, z\n", stderr);
    return 2;
  }
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
    IntegerBoundaries(test);
    asc_helpers_test::EquedFailures(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  const int result = test.Finish();
  if (result == 0) {
    std::printf(
        "LASWP/LAQGE %s: actual typed numerical/workspace/no-INFO checks "
        "passed\n",
        argv[1]);
  }
  return result;
}
