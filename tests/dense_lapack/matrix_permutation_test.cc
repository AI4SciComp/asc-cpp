#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>
#include <thread>

#include "../../src/dense/lapack/internal_tridiagonal.h"
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
#include "matrix_permutation_test_support.h"

namespace {
namespace support = asc_matrix_permutation_test;
using support::Take;
using support::TestContext;

template <typename T>
void Fidelity(TestContext& test, const support::Problem<T>& problem,
              const std::array<T, 1400>& expected) {
  support::Problem<T> native(
      problem.Dimension(), problem.rows ? problem.n : problem.m, 2,
      support::kColumn, problem.rows, problem.direction, 0);
  for (asc::extent_t j = 0; j < problem.n; ++j) {
    for (asc::extent_t i = 0; i < problem.m; ++i) {
      native.dense[native.Index(i, j)] = problem.dense[problem.Index(i, j)];
    }
  }
  std::array<lapack_int, 35> k;
  k.fill(101);
  for (asc::extent_t i = 0; i < problem.Dimension(); ++i) {
    k[static_cast<std::size_t>(i + 1)] = static_cast<lapack_int>(
        problem.indices[static_cast<std::size_t>(i + 1)]);
  }
  const auto before = k;
  const auto matrix_before = native.dense;
  auto native_expected = matrix_before;
  for (asc::extent_t j = 0; j < problem.n; ++j) {
    for (asc::extent_t i = 0; i < problem.m; ++i) {
      native_expected[native.Index(i, j)] = expected[problem.Index(i, j)];
    }
  }
  const lapack_logical forward = problem.direction == support::kForward ? 1 : 0;
  const auto m = static_cast<lapack_int>(problem.m);
  const auto n = static_cast<lapack_int>(problem.n);
  const auto ld = static_cast<lapack_int>(native.leading);
  support::Native(problem.rows, &forward, &m, &n, native.dense.data() + 1, &ld,
                  k.data() + 1);
  ASC_DENSE_TEST_CHECK(test, support::Same(native.dense, native_expected));
  ASC_DENSE_TEST_EQ(test, k, before);
}

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          support::Problem<T>& problem, bool fidelity, bool audit) {
  const auto before = problem.dense;
  const auto indices = problem.indices;
  const auto expected = problem.Expected();
  if (fidelity) {
    Fidelity(test, problem, expected);
  }
  const auto query = [&] { return problem.Query(provider); };
  const auto plan =
      Take(audit ? support::WithoutAllocation(test, query) : query());
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  for (int reuse = 0; reuse < 2; ++reuse) {
    problem.dense = before;
    asc::LapackReport report;
    const auto call = [&] {
      return problem.Run(provider, plan, workspace, report);
    };
    const auto status = audit ? support::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider,
                      problem.m > 0 && problem.n > 0);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, expected));
    ASC_DENSE_TEST_EQ(test, problem.indices, indices);
    scratch.Guards(test, workspace);
  }
}

template <typename T>
void Values(TestContext& test, const asc::ReferenceLapackProvider& provider,
            bool fidelity, bool audit = true, std::size_t first_seed = 0,
            std::size_t seed_count = 12) {
  for (asc::extent_t d : {0, 1, 2, 3, 4, 5, 8, 17, 33}) {
    support::Permutations(d, [&](const auto& k) {
      for (asc::extent_t other : {0, 1, 3, 7}) {
        for (asc::extent_t padding : {0, 2}) {
          for (auto layout : {support::kRow, support::kColumn}) {
            for (bool rows : {false, true}) {
              for (auto direction : {support::kForward, support::kBackward}) {
                for (auto seed = first_seed; seed < first_seed + seed_count;
                     ++seed) {
                  support::Problem<T> problem(d, other, padding, layout, rows,
                                              direction, seed);
                  std::copy_n(k.begin(), d, problem.indices.begin() + 1);
                  Case(test, provider, problem, fidelity, audit);
                }
              }
            }
          }
        }
      }
    });
  }
}

asc::LapackPermutationDirection UnknownDirection() {
  // The fixed uint8_t underlying type permits values outside the enumerators.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<asc::LapackPermutationDirection>(255);
}

template <typename T>
void InvalidIndices(TestContext& test,
                    const asc::ReferenceLapackProvider& provider,
                    support::Problem<T>& problem) {
  const auto matrix = problem.dense;
  const auto original = problem.indices;
  const auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto staged = scratch.staged;
  const auto workspace = scratch.Workspace(plan);
  for (asc::index_t invalid :
       {asc::index_t{0}, asc::index_t{-1}, asc::index_t{4},
        std::numeric_limits<asc::index_t>::min(),
        std::numeric_limits<asc::index_t>::max(), asc::index_t{2}}) {
    problem.indices = original;
    problem.indices[1] = invalid;
    const auto indices = problem.indices;
    ASC_DENSE_TEST_CHECK(test, problem.Query(provider).ok());
    asc::LapackReport report;
    const auto status = support::WithoutAllocation(
        test, [&] { return problem.Run(provider, plan, workspace, report); });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kIndex);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, matrix));
    ASC_DENSE_TEST_EQ(test, problem.indices, indices);
    ASC_DENSE_TEST_CHECK(test, support::Same(scratch.staged, staged));
    scratch.Guards(test, workspace);
  }
  problem.indices = original;
}

template <typename T>
void Validation(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout layout, bool rows,
                asc::LapackPermutationDirection direction) {
  support::Problem<T> problem(3, 3, 2, layout, rows, direction, 1);
  const auto before = problem;
  const auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto scratch_before = scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto reject = [&](const asc::LapackWorkspacePlan& supplied,
                          const asc::LapackWorkspace& space,
                          asc::ErrorCode expected) {
    asc::LapackReport report;
    report.called_provider = true;
    report.native_info = 79;
    const auto status = support::WithoutAllocation(
        test, [&] { return problem.Run(provider, supplied, space, report); });
    ASC_DENSE_TEST_EQ(test, status.code(), expected);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense));
    ASC_DENSE_TEST_EQ(test, problem.indices, before.indices);
    ASC_DENSE_TEST_CHECK(test,
                         support::Same(scratch.staged, scratch_before.staged));
    ASC_DENSE_TEST_EQ(test, scratch.integer, scratch_before.integer);
  };
  auto stale = plan;
  ++stale.regions[support::kScratch].minimum_entries;
  reject(stale, workspace, asc::ErrorCode::kInvalidState);
  for (auto role : {support::kScratch, support::kInteger}) {
    auto short_space = workspace;
    const auto region = workspace.regions[role];
    short_space.regions[role] = {region.data(), region.size() - 1,
                                 region.space()};
    reject(plan, short_space, asc::ErrorCode::kInvalidArgument);
    for (auto memory :
         {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
      auto inaccessible = workspace;
      inaccessible.regions[role] = {region.data(), region.size(), memory};
      reject(plan, inaccessible, asc::ErrorCode::kMemoryAccess);
    }
  }
  auto alias = workspace;
  const auto operand = problem.Matrix().reachable_storage();
  alias.regions[support::kScratch] = {problem.dense.data() + 1, operand.size(),
                                      support::kHost};
  reject(plan, alias, asc::ErrorCode::kInvalidArgument);
  problem.direction = UnknownDirection();
  reject(plan, workspace, asc::ErrorCode::kInvalidArgument);
  problem.direction =
      direction == support::kForward ? support::kBackward : support::kForward;
  reject(plan, workspace, asc::ErrorCode::kInvalidState);
  problem.direction = direction;
  problem.layout = layout == support::kRow ? support::kColumn : support::kRow;
  reject(plan, workspace, asc::ErrorCode::kInvalidState);
  problem.layout = layout;
  problem.rows = !rows;
  reject(plan, workspace, asc::ErrorCode::kInvalidState);
  problem.rows = rows;
  asc::LapackReport report;
  report.native_info = 83;
  const auto report_bytes = std::as_bytes(std::span(&report, 1));
  std::array<std::byte, sizeof(report)> report_before{};
  std::copy(report_bytes.begin(), report_bytes.end(), report_before.begin());
  auto metadata = workspace;
  metadata.regions[support::kInteger] = {&report, sizeof(report),
                                         support::kHost};
  ASC_DENSE_TEST_EQ(test, problem.Run(provider, plan, metadata, report).code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test,
                       std::equal(report_bytes.begin(), report_bytes.end(),
                                  report_before.begin()));
  InvalidIndices(test, provider, problem);
}

template <typename T>
int Run(std::string_view mode) {
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "mathematical" || mode == "fidelity") {
    Values<T>(test, provider, mode == "fidelity");
  } else if (mode == "validation") {
    for (auto layout : {support::kRow, support::kColumn}) {
      for (bool rows : {false, true}) {
        for (auto direction : {support::kForward, support::kBackward}) {
          Validation<T>(test, provider, layout, rows, direction);
        }
      }
    }
  } else if (mode == "concurrency") {
    std::array<int, 4> results{};
    std::array<std::thread, 4> threads;
    for (std::size_t i = 0; i < threads.size(); ++i) {
      threads[i] = std::thread([&, i] {
        TestContext local;
        const auto context = Take(asc::ReferenceLapackProvider::Create(
            asc::ExecutionContext::Serial()));
        Values<T>(local, context, false, false, i, 1);
        results[i] = local.Finish();
      });
    }
    for (auto& thread : threads) {
      thread.join();
    }
    for (const auto result : results) {
      ASC_DENSE_TEST_EQ(test, result, 0);
    }
  } else {
    return 2;
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>(argv[2]);
  }
  if (scalar == "d") {
    return Run<double>(argv[2]);
  }
  if (scalar == "c") {
    return Run<std::complex<float>>(argv[2]);
  }
  if (scalar == "z") {
    return Run<std::complex<double>>(argv[2]);
  }
  return 2;
}
