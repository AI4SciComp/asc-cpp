#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_complete_pivoting_counts.h"
#include "../../src/dense/lapack/internal_layout.h"
#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_complete_pivoting.h"
#include "complete_pivoting_faults.h"
#include "complete_pivoting_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
namespace support = asc_complete_pivoting_test;
namespace fault = asc_complete_pivoting_fault;
using fault::Mode;
using support::Take;
using support::TestContext;
using support::Vector;

void Counts(TestContext& test) {
  using asc::internal_complete_pivoting_counts::Count;
  for (const asc::extent_t limit :
       {asc::extent_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<asc::extent_t>::max()}) {
    const auto bound = limit == std::numeric_limits<std::int32_t>::max()
                           ? asc::extent_t{46340}
                           : asc::extent_t{3037000499};
    for (bool solve : {false, true}) {
      const auto accepted = Take(Count(bound, solve, limit));
      ASC_DENSE_TEST_EQ(test, accepted.scalar,
                        bound * bound + (solve ? bound : 0));
      ASC_DENSE_TEST_EQ(test, accepted.integer, 2 * bound);
      ASC_DENSE_TEST_EQ(test, Take(Count(0, solve, limit)).scalar, 0);
      ASC_DENSE_TEST_EQ(test, Take(Count(0, solve, limit)).integer, 0);
      for (auto invalid : {bound + 1, limit / 2 + 1, limit}) {
        ASC_DENSE_TEST_EQ(test, Count(invalid, solve, limit).status().code(),
                          asc::ErrorCode::kOverflow);
      }
      ASC_DENSE_TEST_EQ(test, Count(-1, solve, limit).status().code(),
                        asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_EQ(test, Count(0, solve, 0).status().code(),
                        asc::ErrorCode::kInvalidArgument);
    }
  }
  // Distinguish the N*N+1 native cursor and N*N+N solve staging bounds.
  ASC_DENSE_TEST_CHECK(test, Count(3, false, 10).ok());
  ASC_DENSE_TEST_EQ(test, Count(3, true, 10).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_EQ(test, Count(3, false, 9).status().code(),
                    asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test, Count(3, true, 12).ok());
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  support::Problem<double> problem(0, 0, support::kColumn, 0, 0);
  auto plan = Take(problem.Query(provider));
  plan.total_byte_limit = 100;
  plan.regions[support::kScratch] = {12, 12, 8, 8};
  plan.regions[support::kInteger] = {1, 1, 8, 8};
  ASC_DENSE_TEST_EQ(test, asc::internal_lapack_layout::CheckTotal(plan).code(),
                    asc::ErrorCode::kOverflow);
}

template <typename T>
void Preserved(TestContext& test, const support::Problem<T>& problem,
               const support::Problem<T>& before) {
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense));
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.rhs, before.rhs));
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.scale, before.scale));
  ASC_DENSE_TEST_EQ(test, problem.rows, before.rows);
  ASC_DENSE_TEST_EQ(test, problem.columns, before.columns);
}

void Entered(TestContext& test, const asc::LapackReport& report,
             asc::ErrorCode actual, asc::ErrorCode expected) {
  ASC_DENSE_TEST_EQ(test, actual, expected);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
  ASC_DENSE_TEST_CHECK(test, fault::BaselinePassed());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    expected == asc::ErrorCode::kOk
                        ? asc::LapackOutputValidity::kComplete
                        : asc::LapackOutputValidity::kUnusable);
}

template <typename T>
void FactorCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout layout, Mode mode) {
  support::Problem<T> problem(3, 2, layout, 0, 0);
  auto expected = problem;
  const auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  fault::Reset(Mode::kPass);
  ASC_DENSE_TEST_CHECK(test,
                       expected.Factor(provider, plan, workspace, report).ok());
  const auto before = problem;
  fault::Reset(mode);
  const auto status = support::WithoutAllocation(
      test, [&] { return problem.Factor(provider, plan, workspace, report); });
  const bool accepted = mode == Mode::kPass || mode == Mode::kOmitValues ||
                        mode == Mode::kWrongValues;
  auto error = accepted ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider;
  if (mode == Mode::kNonfiniteValues) {
    error = asc::ErrorCode::kNumerical;
  }
  Entered(test, report, status.code(), error);
  lapack_int info = 0;
  if (mode == Mode::kOmitInfo) {
    info = std::numeric_limits<lapack_int>::min();
  }
  if (mode == Mode::kNegativeInfo) {
    info = -1;
  }
  if (mode == Mode::kMaximumInfo) {
    info = std::numeric_limits<lapack_int>::max();
  }
  ASC_DENSE_TEST_EQ(test, report.native_info, info);
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                    mode == Mode::kNegativeInfo);
  if (mode == Mode::kNegativeInfo) {
    ASC_DENSE_TEST_EQ(test, report.native_argument, 1);
  }
  auto outcome = accepted ? asc::LapackOutcome::kSuccess
                          : asc::LapackOutcome::kPartialResult;
  if (info < 0) {
    outcome = asc::LapackOutcome::kProviderArgument;
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  if (!accepted) {
    Preserved(test, problem, before);
  } else {
    if (mode == Mode::kOmitValues) {
      expected = before;
      for (std::size_t i = 1; i <= 3; ++i) {
        expected.rows[i] = static_cast<asc::index_t>(i);
        expected.columns[i] = static_cast<asc::index_t>(i);
      }
    } else if (mode == Mode::kWrongValues) {
      for (asc::extent_t j = 0; j < 3; ++j) {
        for (asc::extent_t i = 0; i < 3; ++i) {
          expected.At(i, j) = T{42};
        }
      }
    }
    Preserved(test, problem, expected);
  }
  problem.Guards(test);
  scratch.Guards(test);
}

template <typename T>
void SolveCase(TestContext& test, const asc::ReferenceLapackProvider& provider,
               asc::DenseBlasLayout layout, Mode mode) {
  support::Problem<T> problem(3, 2, layout, 0, 0);
  auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  fault::Reset(Mode::kPass);
  ASC_DENSE_TEST_CHECK(test,
                       problem.Factor(provider, plan, workspace, report).ok());
  const auto factor = Take(problem.View(report));
  plan = Take(asc::QueryGesc2Workspace(provider, factor, Vector(problem.rhs, 3),
                                       Vector(problem.scale, 1)));
  workspace = scratch.Workspace(plan);
  const auto before = problem;
  auto expected = problem;
  ASC_DENSE_TEST_CHECK(
      test, asc::Gesc2(provider, factor, Vector(expected.rhs, 3),
                       Vector(expected.scale, 1), plan, workspace, report)
                .ok());
  fault::Reset(mode);
  const auto status = support::WithoutAllocation(test, [&] {
    return asc::Gesc2(provider, factor, Vector(problem.rhs, 3),
                      Vector(problem.scale, 1), plan, workspace, report);
  });
  const bool accepted = mode == Mode::kPass || mode == Mode::kOmitValues ||
                        mode == Mode::kWrongValues || mode == Mode::kZeroScale;
  auto error = accepted ? asc::ErrorCode::kOk : asc::ErrorCode::kProvider;
  if (mode == Mode::kNonfiniteValues) {
    error = asc::ErrorCode::kNumerical;
  }
  Entered(test, report, status.code(), error);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  auto outcome = accepted ? asc::LapackOutcome::kSuccess
                          : asc::LapackOutcome::kPartialResult;
  if (mode == Mode::kZeroScale) {
    outcome = asc::LapackOutcome::kAccuracyWarning;
  }
  ASC_DENSE_TEST_EQ(test, report.outcome, outcome);
  if (!accepted) {
    Preserved(test, problem, before);
  } else {
    if (mode == Mode::kOmitValues) {
      expected.rhs = before.rhs;
      expected.scale[1] = 1;
    }
    if (mode == Mode::kWrongValues || mode == Mode::kZeroScale) {
      for (std::size_t i = 1; i <= 3; ++i) {
        expected.rhs[i] = mode == Mode::kZeroScale ? T{} : T{42};
      }
    }
    if (mode == Mode::kZeroScale) {
      expected.scale[1] = 0;
    }
    Preserved(test, problem, expected);
  }
  problem.Guards(test);
  scratch.Guards(test);
}

template <typename T>
void Run(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto layout : {support::kRow, support::kColumn}) {
    for (auto mode :
         {Mode::kPass, Mode::kOmitInfo, Mode::kNegativeInfo, Mode::kMaximumInfo,
          Mode::kMissingRow, Mode::kInvalidColumn, Mode::kNonfiniteValues,
          Mode::kOmitValues, Mode::kWrongValues}) {
      FactorCase<T>(test, provider, layout, mode);
    }
    for (auto mode :
         {Mode::kPass, Mode::kOmitScale, Mode::kNegativeScale,
          Mode::kLargeScale, Mode::kZeroScale, Mode::kChangedFactor,
          Mode::kChangedRow, Mode::kChangedColumn, Mode::kNonfiniteValues,
          Mode::kOmitValues, Mode::kWrongValues}) {
      SolveCase<T>(test, provider, layout, mode);
    }
  }
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  TestContext test;
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    Run<float>(test);
  } else if (scalar == "d") {
    Run<double>(test);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test);
  } else if (scalar == "counts") {
    Counts(test);
  } else {
    return 2;
  }
  return test.Finish();
}
