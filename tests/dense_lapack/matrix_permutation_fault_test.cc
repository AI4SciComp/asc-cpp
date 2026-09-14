#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../../src/dense/lapack/internal_layout.h"
#include "../../src/dense/lapack/internal_matrix_permutation_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_permutation.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_permutation_faults.h"
#include "matrix_permutation_test_support.h"

namespace {
namespace support = asc_matrix_permutation_test;
namespace fault = asc_matrix_permutation_fault;
using fault::Mode;
using support::Take;
using support::TestContext;

void Counts(TestContext& test) {
  using asc::internal_matrix_permutation_counts::Count;
  for (const asc::extent_t limit :
       {asc::extent_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<asc::extent_t>::max()}) {
    const auto bound = limit == std::numeric_limits<std::int32_t>::max()
                           ? asc::extent_t{46340}
                           : asc::extent_t{3037000499};
    const auto result = Count(bound, bound, limit);
    ASC_DENSE_TEST_CHECK(test, result.ok());
    if (result.ok()) {
      ASC_DENSE_TEST_EQ(test, *result, bound * bound);
    }
    ASC_DENSE_TEST_CHECK(test, Count(1, limit - 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, Count(limit - 1, 1, limit).ok());
    ASC_DENSE_TEST_EQ(test, Count(bound + 1, bound + 1, limit).status().code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Count(1, limit, limit).status().code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Count(limit, 1, limit).status().code(),
                      asc::ErrorCode::kOverflow);
    ASC_DENSE_TEST_EQ(test, Count(-1, 1, limit).status().code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, Count(1, -1, limit).status().code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, Take(Count(0, limit, limit)), 0);
    ASC_DENSE_TEST_EQ(test, Take(Count(limit, 0, limit)), 0);
  }
  ASC_DENSE_TEST_EQ(test, Count(0, 0, 0).status().code(),
                    asc::ErrorCode::kInvalidArgument);
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  support::Problem<double> empty(0, 0, 0, support::kColumn, true,
                                 support::kForward, 0);
  auto plan = Take(empty.Query(provider));
  plan.total_byte_limit = 100;
  plan.regions[support::kScratch] = {12, 12, 8, 8};
  plan.regions[support::kInteger] = {1, 1, 8, 8};
  ASC_DENSE_TEST_EQ(test, asc::internal_lapack_layout::CheckTotal(plan).code(),
                    asc::ErrorCode::kOverflow);
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::DenseBlasLayout layout, bool rows,
          asc::LapackPermutationDirection direction, Mode mode) {
  support::Problem<T> problem(3, 3, 2, layout, rows, direction, 1);
  problem.indices[1] = 3;
  problem.indices[2] = 1;
  problem.indices[3] = 2;
  const auto before = problem;
  const bool defect = mode == Mode::kNegativeIndex ||
                      mode == Mode::kChangedPermutation ||
                      mode == Mode::kMaximumIndex;
  auto expected = problem.Expected();
  if (defect || mode == Mode::kOmitValues) {
    expected = before.dense;
  }
  if (mode == Mode::kWrongValues) {
    for (asc::extent_t j = 0; j < 3; ++j) {
      for (asc::extent_t i = 0; i < 3; ++i) {
        expected[problem.Index(i, j)] = T{42};
      }
    }
  }
  const auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  fault::Reset(mode);
  const auto status = support::WithoutAllocation(
      test, [&] { return problem.Run(provider, plan, workspace, report); });
  ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{1});
  ASC_DENSE_TEST_CHECK(test, fault::BaselinePassed());
  ASC_DENSE_TEST_EQ(test, status.code(),
                    defect ? asc::ErrorCode::kProvider : asc::ErrorCode::kOk);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    defect ? asc::LapackOutcome::kPartialResult
                           : asc::LapackOutcome::kSuccess);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    defect ? asc::LapackOutputValidity::kUnusable
                           : asc::LapackOutputValidity::kComplete);
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, expected));
  ASC_DENSE_TEST_EQ(test, problem.indices, before.indices);
  scratch.Guards(test, workspace);
}

template <typename T>
void NoEntry(TestContext& test, const asc::ReferenceLapackProvider& provider,
             asc::DenseBlasLayout layout, bool rows,
             asc::LapackPermutationDirection direction) {
  // An invalid permutation is rejected after metadata-only query, before any
  // foreign call. This observes entry without handing malformed K to Fortran.
  for (asc::extent_t d : {1, 3}) {
    support::Problem<T> problem(d, 3, 2, layout, rows, direction, 1);
    problem.indices[1] = 0;
    const auto before = problem;
    const auto plan = Take(problem.Query(provider));
    support::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    fault::Reset(Mode::kPass);
    ASC_DENSE_TEST_EQ(test,
                      problem.Run(provider, plan, workspace, report).code(),
                      asc::ErrorCode::kIndex);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense));
    ASC_DENSE_TEST_EQ(test, problem.indices, before.indices);
  }
  // Empty matrices promise local completion without reading index values.
  for (asc::extent_t d : {0, 1, 3}) {
    support::Problem<T> empty(d, 0, 2, layout, rows, direction, 1);
    empty.indices.fill(std::numeric_limits<asc::index_t>::min());
    const auto before = empty;
    const auto plan = Take(empty.Query(provider));
    support::Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    fault::Reset(Mode::kPass);
    ASC_DENSE_TEST_CHECK(test,
                         empty.Run(provider, plan, workspace, report).ok());
    ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::Same(empty.dense, before.dense));
    ASC_DENSE_TEST_EQ(test, empty.indices, before.indices);
    scratch.Guards(test, workspace);
  }
}

template <typename T>
void InputValidation(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     asc::DenseBlasLayout layout, bool rows,
                     asc::LapackPermutationDirection direction) {
  support::Problem<T> problem(3, 3, 2, layout, rows, direction, 1);
  const auto before = problem;
  const auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto scratch_before = scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto reject = [&](asc::DenseBlasMatrixView<T> matrix,
                          asc::DenseBlasVectorView<const asc::index_t> indices,
                          asc::ErrorCode expected) {
    asc::LapackReport report;
    report.native_info = 83;
    fault::Reset(Mode::kPass);
    const auto status = support::WithoutAllocation(test, [&] {
      return rows ? asc::Lapmr(provider, direction, matrix, indices, plan,
                               workspace, report)
                  : asc::Lapmt(provider, direction, matrix, indices, plan,
                               workspace, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), std::size_t{0});
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense));
    ASC_DENSE_TEST_EQ(test, problem.indices, before.indices);
    ASC_DENSE_TEST_CHECK(test,
                         support::Same(scratch.staged, scratch_before.staged));
    ASC_DENSE_TEST_EQ(test, scratch.integer, scratch_before.integer);
  };
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    const auto matrix = Take(asc::DenseBlasMatrixView<T>::Create(
        problem.dense.data() + 1, 3, 3, layout, problem.leading,
        {problem.dense.data(), sizeof(problem.dense), space}));
    reject(matrix, problem.Permutation(), asc::ErrorCode::kMemoryAccess);
    const auto indices =
        Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
            problem.indices.data() + 1, 3, 1,
            {problem.indices.data(), sizeof(problem.indices), space}));
    reject(problem.Matrix(), indices, asc::ErrorCode::kMemoryAccess);
  }
  for (asc::extent_t size : {2, 4}) {
    const auto indices =
        Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
            problem.indices.data() + 1, size, 1,
            {problem.indices.data(), sizeof(problem.indices), support::kHost}));
    reject(problem.Matrix(), indices, asc::ErrorCode::kShape);
  }
  const auto strided =
      Take(asc::DenseBlasVectorView<const asc::index_t>::Create(
          problem.indices.data() + 1, 3, 2,
          {problem.indices.data(), sizeof(problem.indices), support::kHost}));
  reject(problem.Matrix(), strided, asc::ErrorCode::kShape);
}

template <typename T>
void Run(TestContext& test) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto layout : {support::kRow, support::kColumn}) {
    for (bool rows : {false, true}) {
      for (auto direction : {support::kForward, support::kBackward}) {
        for (auto mode :
             {Mode::kPass, Mode::kNegativeIndex, Mode::kChangedPermutation,
              Mode::kMaximumIndex, Mode::kOmitValues, Mode::kWrongValues}) {
          Case<T>(test, provider, layout, rows, direction, mode);
        }
        NoEntry<T>(test, provider, layout, rows, direction);
        InputValidation<T>(test, provider, layout, rows, direction);
      }
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
