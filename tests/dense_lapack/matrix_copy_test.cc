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
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_matrix_copy.h"
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
          Part part, asc::extent_t m, asc::extent_t n, asc::DenseBlasLayout il,
          asc::DenseBlasLayout ol, bool audit = true) {
  support::Problem<T> problem(m, n, il, ol);
  problem.part = part;
  for (asc::extent_t i = 0; i < m; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      problem.input.At(i, j) = Fixture<T>(i, j);
    }
  }
  const auto before = problem;
  const auto plan = Take(problem.Query(provider));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  for (int reuse = 0; reuse < 2; ++reuse) {
    asc::LapackReport report;
    const auto call = [&] {
      return problem.Run(provider, plan, workspace, report);
    };
    const auto status = audit ? support::WithoutAllocation(test, call) : call();
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, report.called_provider, m > 0 && n > 0);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    for (asc::extent_t i = 0; i < m; ++i) {
      for (asc::extent_t j = 0; j < n; ++j) {
        const auto diagonal_offset = i - j;
        const bool selected =
            part == Part::kAll || (part == Part::kUpper ? diagonal_offset <= 0
                                                        : diagonal_offset >= 0);
        const auto expected =
            selected ? before.input.At(i, j) : before.output.At(i, j);
        ASC_DENSE_TEST_CHECK(test,
                             SameBytes(problem.output.At(i, j), expected));
      }
    }
  }
  ASC_DENSE_TEST_CHECK(test, SameBytes(problem.input.data, before.input.data));
  problem.input.Guards(test);
  problem.output.Guards(test);
  scratch.Guards(test, workspace);
}
template <typename T>
void Validation(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout il, asc::DenseBlasLayout ol) {
  support::Problem<T> p(3, 2, il, ol);
  p.part = Part::kUpper;
  const auto before = p;
  const auto plan = Take(p.Query(provider));
  support::Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report;
  for (auto role : {support::kLayout, support::kScratch}) {
    if (workspace.regions[role].size() == 0) {
      continue;
    }
    auto short_work = workspace;
    short_work.regions[role] = {workspace.regions[role].data(),
                                workspace.regions[role].size() - 1,
                                support::kHost};
    ASC_DENSE_TEST_CHECK(test, !p.Run(provider, plan, short_work, report).ok());
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
  auto stale = plan;
  stale.regions[support::kScratch].minimum_entries = 0;
  ASC_DENSE_TEST_CHECK(test, !p.Run(provider, stale, workspace, report).ok());
  p.part = Part::kLower;
  ASC_DENSE_TEST_CHECK(test, !p.Run(provider, plan, workspace, report).ok());
  // Fixed uint8_t enum:99 is representable but deliberately not an enumerator.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  p.part = static_cast<Part>(99);
  ASC_DENSE_TEST_CHECK(test, !p.Query(provider).ok());
  p.part = Part::kUpper;
  auto alias = workspace;
  alias.regions[support::kScratch] = {
      p.output.data.data(), workspace.regions[support::kScratch].size(),
      support::kHost};
  ASC_DENSE_TEST_CHECK(test, !p.Run(provider, plan, alias, report).ok());
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  ASC_DENSE_TEST_CHECK(test, SameBytes(p.input.data, before.input.data));
  ASC_DENSE_TEST_CHECK(test, SameBytes(p.output.data, before.output.data));
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
  for (auto il : {support::kColumn, support::kRow}) {
    for (auto ol : {support::kColumn, support::kRow}) {
      for (auto part : {Part::kAll, Part::kUpper, Part::kLower}) {
        for (asc::extent_t m : {0, 1, 2, 5}) {
          for (asc::extent_t n : {0, 1, 3, 4}) {
            Case<T>(test, provider, part, m, n, il, ol);
          }
        }
      }
      Validation<T>(test, provider, il, ol);
    }
  }
  std::array<int, 4> failures{};
  std::array<std::thread, 4> threads;
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i] = std::thread([&, i] {
      TestContext local;
      const auto independent = Take(asc::ReferenceLapackProvider::Create(
          asc::ExecutionContext::Serial()));
      for (int repetition = 0; repetition < 8; ++repetition) {
        Case<T>(local, independent, static_cast<Part>(i % 3), 3, 2,
                i % 2 == 0 ? support::kRow : support::kColumn,
                i % 2 == 0 ? support::kColumn : support::kRow, false);
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
