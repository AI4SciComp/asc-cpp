#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <thread>

#include "../../src/dense/lapack/internal_matrix_copy_counts.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
#include "asc/dense/providers/lapack_matrix_set.h"
#include "installed_lu/normal_return_guard.h"
#include "matrix_copy_test_support.h"
namespace {
namespace support = asc_copy_test;
using support::Take;
using support::TestContext;
using Part = asc::LapackMatrixPart;
template <typename T>
bool SameBytes(const T& a, const T& b) {
  const auto first = std::as_bytes(std::span(&a, 1));
  const auto second = std::as_bytes(std::span(&b, 1));
  return std::equal(first.begin(), first.end(), second.begin());
}
template <typename T>
T Fixture(asc::extent_t i, asc::extent_t j) {
  using Real = asc::DenseBlasRealType<T>;
  const std::array<Real, 8> values{Real{0},
                                   -Real{0},
                                   Real{0.25},
                                   -Real{3},
                                   std::numeric_limits<Real>::denorm_min(),
                                   std::numeric_limits<Real>::max(),
                                   std::numeric_limits<Real>::infinity(),
                                   std::numeric_limits<Real>::quiet_NaN()};
  const auto index = static_cast<std::size_t>(i + 3 * j);
  return support::Value<T>(values[index % values.size()],
                           values[(index + 3) % values.size()]);
}
template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          Part part, asc::extent_t m, asc::extent_t n,
          asc::DenseBlasLayout layout, bool audit = true,
          const asc::LapackWorkspacePlan* reusable = nullptr) {
  support::Rhs<T> output(m, n, layout);
  const auto before = output;
  const auto queried =
      Take(asc::QueryLasetWorkspace(provider, part, output.View()));
  const auto& plan = reusable == nullptr ? queried : *reusable;
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto expected_count = layout == support::kRow ? m * n : 0;
  ASC_DENSE_TEST_EQ(test, plan.regions[support::kLayout].minimum_entries,
                    expected_count);
  // Reuse one immutable plan with all independent alpha/beta value classes.
  for (asc::extent_t ai = 0; ai < 8; ++ai) {
    for (asc::extent_t bi = 0; bi < 8; ++bi) {
      const auto alpha = Fixture<T>(ai, 0);
      const auto beta = Fixture<T>(bi, 0);
      asc::LapackReport report;
      const auto call = [&] {
        return asc::Laset(provider, part, alpha, beta, output.View(), plan,
                          workspace, report);
      };
      const auto status =
          audit ? support::WithoutAllocation(test, call) : call();
      ASC_DENSE_TEST_CHECK(test, status.ok());
      ASC_DENSE_TEST_EQ(test, report.called_provider, m > 0 && n > 0);
      ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kComplete);
      for (asc::extent_t i = 0; i < m; ++i) {
        for (asc::extent_t j = 0; j < n; ++j) {
          const bool selected =
              part == Part::kAll || (part == Part::kUpper ? i <= j : i >= j);
          auto expected = before.At(i, j);
          if (selected) {
            expected = i == j ? beta : alpha;
          }
          ASC_DENSE_TEST_CHECK(test, SameBytes(output.At(i, j), expected));
        }
      }
    }
  }
  output.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
void Validation(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout layout) {
  support::Rhs<T> output(3, 2, layout);
  const auto before = output;
  const auto part = Part::kUpper;
  const auto plan =
      Take(asc::QueryLasetWorkspace(provider, part, output.View()));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  const auto run = [&](const auto& selected, const auto& work) {
    return asc::Laset(provider, part, T{3}, T{-2}, output.View(), selected,
                      work, report);
  };
  if (layout == support::kRow) {
    auto short_work = workspace;
    short_work.regions[support::kLayout] = {
        workspace.regions[support::kLayout].data(),
        workspace.regions[support::kLayout].size() - 1, support::kHost};
    ASC_DENSE_TEST_EQ(test, run(plan, short_work).code(),
                      asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
    auto alias = workspace;
    alias.regions[support::kLayout] = {
        output.data.data(), workspace.regions[support::kLayout].size(),
        support::kHost};
    ASC_DENSE_TEST_CHECK(test, !run(plan, alias).ok());
  }
  auto stale = plan;
  ++stale.regions[support::kLayout].minimum_entries;
  ASC_DENSE_TEST_CHECK(test, !run(stale, workspace).ok());
  ASC_DENSE_TEST_CHECK(test, !asc::Laset(provider, Part::kLower, T{3}, T{-2},
                                         output.View(), plan, workspace, report)
                                  .ok());
  // Fixed uint8_t enum:99 is representable but deliberately not an enumerator.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  const auto invalid = static_cast<Part>(99);
  ASC_DENSE_TEST_CHECK(
      test, !asc::QueryLasetWorkspace(provider, invalid, output.View()).ok());
  ASC_DENSE_TEST_CHECK(test, !asc::Laset(provider, invalid, T{3}, T{-2},
                                         output.View(), plan, workspace, report)
                                  .ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(test, SameBytes(output.data, before.data));
}
void Counts(TestContext& test) {
  namespace counts = asc::internal_matrix_copy_counts;
  for (const std::int64_t limit :
       {std::int64_t{std::numeric_limits<std::int32_t>::max()},
        std::numeric_limits<std::int64_t>::max()}) {
    ASC_DENSE_TEST_CHECK(test, counts::Count(1, 1, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Count(limit, 0, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Count(limit, 1, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Count(1, limit, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, counts::Count(1, limit - 1, 1, limit).ok());
    ASC_DENSE_TEST_CHECK(test, !counts::Count(-1, 2, 1, limit).ok());
  }
}
template <typename T>
int Run() {
  TestContext test;
  Counts(test);
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  for (auto layout : {support::kColumn, support::kRow}) {
    for (auto part : {Part::kAll, Part::kUpper, Part::kLower}) {
      for (asc::extent_t m : {0, 1, 2, 5}) {
        for (asc::extent_t n : {0, 1, 3, 4}) {
          Case<T>(test, provider, part, m, n, layout);
        }
      }
    }
    Validation<T>(test, provider, layout);
  }
  support::Rhs<T> row_shape(3, 2, support::kRow);
  support::Rhs<T> column_shape(3, 2, support::kColumn);
  const std::array shared_plans{
      Take(asc::QueryLasetWorkspace(provider, Part::kAll, row_shape.View())),
      Take(
          asc::QueryLasetWorkspace(provider, Part::kAll, column_shape.View()))};
  std::array<int, 4> failures{};
  std::array<std::thread, 4> threads;
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      TestContext local;
      const auto independent = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      for (int repetition = 0; repetition < 8; ++repetition) {
        Case<T>(local, independent, Part::kAll, 3, 2,
                i % 2 == 0 ? support::kRow : support::kColumn, false,
                &shared_plans[i % 2]);
      }
      failures[i] = local.Finish();
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  for (auto failure : failures) {
    ASC_DENSE_TEST_EQ(test, failure, 0);
  }
  return test.Finish();
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 2) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  if (scalar == "s") {
    return Run<float>();
  }
  if (scalar == "d") {
    return Run<double>();
  }
  if (scalar == "c") {
    return Run<std::complex<float>>();
  }
  if (scalar == "z") {
    return Run<std::complex<double>>();
  }
  return 2;
}
