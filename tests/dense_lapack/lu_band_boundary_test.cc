#include <complex>
#include <cstddef>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_faults.h"
#include "lu_band_test_support.h"

namespace {
namespace support = asc_lu_band_test;
namespace faults = asc_lu_band_faults;
using support::Take;
using support::TestContext;
using support::WithoutAllocation;

template <typename T>
void FactorAlias(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(3, 3, 1, 1);
  support::Pivots pivots(3);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan = Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto original_workspace = scratch.View();
  const auto band_before = band.values;
  const auto pivots_before = pivots.values;
  for (const bool alias_band : {false, true}) {
    auto workspace = original_workspace;
    const std::size_t bytes = workspace.regions[support::kInteger].size();
    // Both bases have native alignment and sufficient real backing/capacity.
    // A failure for short or unaligned scratch cannot mask the alias check.
    void* data = alias_band ? static_cast<void*>(matrix.storage().data())
                            : static_cast<void*>(pivots.values.data() + 1);
    workspace.regions[support::kInteger] = {data, bytes, support::kHost};
    faults::Select(faults::Fault::kLargePositive);
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gbtrf(provider, matrix, swaps, plan, workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(band.values, band_before));
    ASC_DENSE_TEST_EQ(test, pivots.values, pivots_before);
    scratch.Check(test);
  }
}

template <typename T>
void SolveAlias(TestContext& test,
                const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(3, 3, 1, 1);
  support::Pivots pivots(3);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan = Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  faults::Select(faults::Fault::kPass);
  asc::LapackReport factor_report;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::Gbtrf(provider, matrix, swaps, plan, workspace, factor_report).ok());
  const auto factor = Take(asc::ReferenceLuBandFactorView<T>::Create(
      provider, band.ConstView(), pivots.ConstView(), factor_report));
  const auto band_before = band.values;
  const auto pivots_before = pivots.values;
  for (const auto operation : support::kOperations) {
    support::Rhs<T> rhs(band, 1, support::kColumn, operation);
    const auto clean = Take(asc::DenseBlasMatrixView<T>::Create(
        rhs.values.data() + 1, 3, 1, support::kColumn, 3,
        {rhs.values.data(), rhs.values.size() * sizeof(T), support::kHost}));
    const auto alias = Take(asc::DenseBlasMatrixView<T>::Create(
        matrix.storage().data(), 3, 1, support::kColumn, 3,
        {band.values.data(), band.values.size() * sizeof(T), support::kHost}));
    const auto solve_plan =
        Take(asc::QueryGbtrsWorkspace(provider, operation, factor, clean));
    support::Scratch<T> solve_scratch(solve_plan);
    const auto solve_workspace = solve_scratch.View();
    faults::Select(faults::Fault::kLargePositive);
    const auto query = WithoutAllocation(test, [&] {
      return asc::QueryGbtrsWorkspace(provider, operation, factor, alias);
    });
    ASC_DENSE_TEST_EQ(test, query.status().code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    // The clean and aliased RHS have exactly the same plan-bound metadata.
    // A stale-plan rejection therefore cannot mask a lost overlap check.
    asc::LapackReport report;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gbtrs(provider, operation, factor, alias, solve_plan,
                        solve_workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(band.values, band_before));
    ASC_DENSE_TEST_EQ(test, pivots.values, pivots_before);
    solve_scratch.Check(test);
  }
}

template <typename T>
void OnePivotWidth(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(1, 1, 0, 0);
  support::Pivots pivots(1);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan = Take(asc::QueryGbtrfWorkspace(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  const auto before = pivots.values;
  faults::Select(faults::Fault::kPartialWidth);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return asc::Gbtrf(provider, matrix, swaps, plan, workspace, report);
  });
  // For actual ILP64 the fault writes valid pivot 1 into just the low four
  // bytes, leaving the native high sentinel. There are no other pivots whose
  // unwritten sentinel could mask that width defect. LP64 tests an unwritten
  // complete pivot, because a half-width ILP64 result is not an LP64 mode.
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_EQ(test, pivots.values, before);
  scratch.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  FactorAlias<T>(test, provider);
  SolveAlias<T>(test, provider);
  OnePivotWidth<T>(test, provider);
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test, provider);
  } else if (scalar == "d") {
    Run<double>(test, provider);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, provider);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, provider);
  } else {
    return 2;
  }
  return test.Finish();
}
