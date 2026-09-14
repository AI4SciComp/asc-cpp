#include <array>
#include <complex>
#include <cstddef>
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
#include "asc/dense/providers/lapack_triangular_storage_conversion.h"
#include "installed_lu/normal_return_guard.h"
#include "triangular_storage_conversion_test_support.h"

namespace {
namespace support = asc_triangular_storage_test;
using support::Take;
using support::TestContext;

template <typename T>
void Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
          asc::extent_t n, asc::extent_t padding, asc::DenseBlasLayout dl,
          asc::DenseBlasLayout pl, asc::DenseBlasTriangle triangle, bool pack,
          std::size_t seed, bool fidelity, bool audit = true) {
  support::Problem<T> problem(n, padding, dl, pl, triangle, pack, seed);
  auto expected = problem;
  for (asc::extent_t i = 0; i < n; ++i) {
    for (asc::extent_t j = 0; j < n; ++j) {
      if (support::Selected(triangle, i, j)) {
        const auto value = support::Fixture<T>(i, j, seed);
        if (pack) {
          expected.packed[1 + support::PackedIndex(n, triangle, pl, i, j)] =
              value;
        } else {
          expected.dense[expected.DenseIndex(i, j)] = value;
        }
      }
    }
  }
  support::Problem<T> native(n, padding, support::kColumn, support::kColumn,
                             triangle, pack, seed);
  if (fidelity) {
    auto size = static_cast<lapack_int>(n);
    auto leading = static_cast<lapack_int>(native.leading);
    lapack_int info = 79;
    char uplo = triangle == support::kUpper ? 'U' : 'L';
    support::Native(pack, &uplo, &size, native.dense.data() + 1, &leading,
                    native.packed.data() + 1, &info);
    ASC_DENSE_TEST_EQ(test, info, 0);
  }
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
    ASC_DENSE_TEST_EQ(test, report.called_provider, n > 0);
    ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), n > 0);
    if (n > 0) {
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-79), 0);
    }
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kComplete);
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, expected.dense));
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.packed, expected.packed));
    if (fidelity) {
      for (asc::extent_t i = 0; i < n; ++i) {
        for (asc::extent_t j = 0; j < n; ++j) {
          if (support::Selected(triangle, i, j)) {
            const auto actual =
                pack ? problem.packed[1 + support::PackedIndex(n, triangle, pl,
                                                               i, j)]
                     : problem.dense[problem.DenseIndex(i, j)];
            const auto direct =
                pack ? native
                           .packed[1 + support::PackedIndex(
                                           n, triangle, support::kColumn, i, j)]
                     : native.dense[native.DenseIndex(i, j)];
            ASC_DENSE_TEST_CHECK(test, support::Same(actual, direct));
          }
        }
      }
    }
    scratch.Guards(test, workspace);
  }
}

asc::DenseBlasTriangle UnknownTriangle() {
  // Fixed underlying uint8_t permits255; exercise unknown-enumerator rejection.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return static_cast<asc::DenseBlasTriangle>(255);
}

template <typename T, typename Reject>
void MemoryPlacement(TestContext& test,
                     const asc::ReferenceLapackProvider& provider,
                     support::Problem<T>& problem,
                     const asc::LapackWorkspacePlan& plan,
                     const asc::LapackWorkspace& workspace, Reject reject) {
  const auto before = problem;
  const auto dl = problem.dense_layout;
  const auto pl = problem.packed_layout;
  const auto triangle = problem.triangle;
  const bool pack = problem.to_packed;
  for (auto space : {asc::MemorySpace::kDevice, asc::MemorySpace::kManaged}) {
    auto inaccessible = workspace;
    const auto region = workspace.regions[support::kScratch];
    inaccessible.regions[support::kScratch] = {region.data(), region.size(),
                                               space};
    reject(plan, inaccessible, asc::ErrorCode::kMemoryAccess);
    asc::LapackReport inaccessible_report;
    const auto status = support::WithoutAllocation(test, [&] {
      if (pack) {
        const auto input = Take(asc::DenseBlasMatrixView<const T>::Create(
            problem.dense.data() + 1, problem.n, problem.n, dl, problem.leading,
            {problem.dense.data(), sizeof(problem.dense), space}));
        return asc::Trttp(provider, triangle, input, problem.Packed(), plan,
                          workspace, inaccessible_report);
      }
      const auto input = Take(asc::DenseBlasPackedMatrixView<const T>::Create(
          problem.packed.data() + 1, problem.n, pl,
          {problem.packed.data(), sizeof(problem.packed), space}));
      return asc::Tpttr(provider, triangle, input, problem.Dense(), plan,
                        workspace, inaccessible_report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
    ASC_DENSE_TEST_CHECK(test, !inaccessible_report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !inaccessible_report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense));
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.packed, before.packed));
  }
}

template <typename T>
void Validation(TestContext& test, const asc::ReferenceLapackProvider& provider,
                asc::DenseBlasLayout dl, asc::DenseBlasLayout pl,
                asc::DenseBlasTriangle triangle, bool pack) {
  support::Problem<T> problem(3, 2, dl, pl, triangle, pack, 1);
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
    ASC_DENSE_TEST_CHECK(test, support::Same(problem.packed, before.packed));
    ASC_DENSE_TEST_CHECK(test, support::Same(scratch, scratch_before));
  };
  auto stale = plan;
  ++stale.regions[support::kScratch].minimum_entries;
  reject(stale, workspace, asc::ErrorCode::kInvalidState);
  for (auto role : {support::kLayout, support::kScratch}) {
    if (workspace.regions[role].size() > 0) {
      auto short_space = workspace;
      const auto region = workspace.regions[role];
      short_space.regions[role] = {region.data(), region.size() - 1,
                                   region.space()};
      reject(plan, short_space, asc::ErrorCode::kInvalidArgument);
    }
  }
  auto alias = workspace;
  const auto operand = problem.Dense().reachable_storage();
  alias.regions[support::kScratch] = {problem.dense.data() + 1, operand.size(),
                                      support::kHost};
  reject(plan, alias, asc::ErrorCode::kInvalidArgument);
  problem.triangle = UnknownTriangle();
  reject(plan, workspace, asc::ErrorCode::kInvalidArgument);
  problem.triangle = triangle;
  problem.triangle =
      triangle == support::kUpper ? support::kLower : support::kUpper;
  reject(plan, workspace, asc::ErrorCode::kInvalidState);
  problem.triangle = triangle;
  problem.dense_layout = dl == support::kRow ? support::kColumn : support::kRow;
  reject(plan, workspace, asc::ErrorCode::kInvalidState);
  problem.dense_layout = dl;
  problem.packed_layout =
      pl == support::kRow ? support::kColumn : support::kRow;
  reject(plan, workspace, asc::ErrorCode::kInvalidState);
  problem.packed_layout = pl;

  MemoryPlacement(test, provider, problem, plan, workspace, reject);

  // The report is a live report object; the workspace merely describes its
  // bytes. Rejection must happen before either report reset or numeric access.
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 79;
  auto metadata = workspace;
  metadata.regions[support::kScratch] = {&report, sizeof(report),
                                         support::kHost};
  ASC_DENSE_TEST_EQ(
      test,
      support::WithoutAllocation(
          test, [&] { return problem.Run(provider, plan, metadata, report); })
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-79), 79);
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.dense, before.dense));
  ASC_DENSE_TEST_CHECK(test, support::Same(problem.packed, before.packed));
  ASC_DENSE_TEST_CHECK(test, support::Same(scratch, scratch_before));
}

template <typename T>
void Run(TestContext& test, std::string_view mode) {
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  if (mode == "validation") {
    for (auto dl : {support::kRow, support::kColumn}) {
      for (auto pl : {support::kRow, support::kColumn}) {
        for (auto triangle : {support::kUpper, support::kLower}) {
          for (bool pack : {false, true}) {
            Validation<T>(test, provider, dl, pl, triangle, pack);
          }
        }
      }
    }
    return;
  }
  const auto work = [&](TestContext& context, std::size_t first,
                        std::size_t last, bool audit) {
    const auto local_provider = Take(
        asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
    for (auto n : support::kOrders) {
      for (asc::extent_t padding : {0, 1, 5}) {
        for (auto dl : {support::kRow, support::kColumn}) {
          for (auto pl : {support::kRow, support::kColumn}) {
            for (auto triangle : {support::kUpper, support::kLower}) {
              for (bool pack : {false, true}) {
                for (auto seed = first; seed < last; ++seed) {
                  Case<T>(context, local_provider, n, padding, dl, pl, triangle,
                          pack, seed, mode == "fidelity", audit);
                }
              }
            }
          }
        }
      }
    }
  };
  if (mode == "concurrency") {
    std::array<TestContext, 4> contexts;
    std::array<std::thread, 4> threads;
    for (std::size_t i = 0; i < threads.size(); ++i) {
      threads[i] = std::thread([&, i] { work(contexts[i], i, i + 1, false); });
    }
    for (std::size_t i = 0; i < threads.size(); ++i) {
      threads[i].join();
      ASC_DENSE_TEST_EQ(test, contexts[i].Finish(), 0);
    }
  } else {
    work(test, 0, 16, true);
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard guard;
  if (argc != 3) {
    return 2;
  }
  const std::string_view scalar(argv[1]);
  const std::string_view mode(argv[2]);
  if (mode != "mathematical" && mode != "fidelity" && mode != "validation" &&
      mode != "concurrency") {
    return 2;
  }
  TestContext test;
  if (scalar == "s") {
    Run<float>(test, mode);
  } else if (scalar == "d") {
    Run<double>(test, mode);
  } else if (scalar == "c") {
    Run<std::complex<float>>(test, mode);
  } else if (scalar == "z") {
    Run<std::complex<double>>(test, mode);
  } else {
    return 2;
  }
  return test.Finish();
}
