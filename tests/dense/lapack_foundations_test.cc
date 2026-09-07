#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>

#include "../allocation_observation.h"
#include "allocation_probe.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;

asc::LapackPlanIdentity MakeIdentity(
    std::string_view routine = "dgetrf",
    asc::LapackScalarKind scalar = asc::LapackScalarKind::kF64,
    asc::extent_t rows = 3, std::int64_t option = 0,
    asc::LapackProviderIdentity provider = {}) {
  const std::array<asc::extent_t, 2> dimensions{rows, 3};
  const std::array<std::int64_t, 1> options{option};
  auto result = asc::LapackPlanIdentity::Create(routine, scalar, dimensions,
                                                options, provider);
  if (!result.ok()) {
    std::abort();
  }
  return *result;
}

void TestIdentity(TestContext& test) {
  std::array<asc::extent_t, 2> dimensions{3, 3};
  std::array<std::int64_t, 1> options{0};
  auto copied = asc::LapackPlanIdentity::Create(
      "dgetrf", asc::LapackScalarKind::kF64, dimensions, options, {});
  ASC_DENSE_TEST_CHECK(test, copied.ok());
  dimensions[0] = 7;
  options[0] = 4;
  ASC_DENSE_TEST_CHECK(test, *copied == MakeIdentity());
  ASC_DENSE_TEST_CHECK(test, *copied != MakeIdentity("dpotrf"));
  ASC_DENSE_TEST_CHECK(
      test, *copied != MakeIdentity("dgetrf", asc::LapackScalarKind::kC128));
  ASC_DENSE_TEST_CHECK(
      test, *copied != MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 4));
  ASC_DENSE_TEST_CHECK(
      test,
      *copied != MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 3, 1));
  std::array<asc::extent_t, 17> excess{};
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackPlanIdentity::Create(
                 "dgetrf", asc::LapackScalarKind::kF64, excess, {}, {})
                 .ok());
  std::array<std::int64_t, 17> excess_options{};
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackPlanIdentity::Create(
                 "dgetrf", asc::LapackScalarKind::kF64, {}, excess_options, {})
                 .ok());
  ASC_DENSE_TEST_CHECK(test, !asc::LapackPlanIdentity::Create(
                                  "", asc::LapackScalarKind::kF64, {}, {}, {})
                                  .ok());
  dimensions[0] = -1;
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackPlanIdentity::Create(
                 "dgetrf", asc::LapackScalarKind::kF64, dimensions, {}, {})
                 .ok());
}

void TestProviderIdentity(TestContext& test) {
  asc::LapackProviderIdentity provider;
  provider.kind = asc::LapackProviderKind::kReference;
  provider.integer_abi = asc::LapackIntegerAbi::kLp64;
  provider.logical_bytes = 4;
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackPlanIdentity::Create(
                 "dgetrf", asc::LapackScalarKind::kF64, {}, {}, provider)
                 .ok());
  // Synthetic metadata tests plan invalidation; these bytes are not a provider.
  provider.source_sha256[0] = std::byte{1};
  provider.build_sha256[0] = std::byte{2};
  const auto original =
      MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 3, 0, provider);
  provider.build_sha256[1] = std::byte{3};
  ASC_DENSE_TEST_CHECK(
      test, original != MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 3,
                                     0, provider));
  provider.build_sha256[1] = std::byte{0};
  provider.integer_abi = asc::LapackIntegerAbi::kIlp64;
  const auto ilp64 =
      MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 3, 0, provider);
  ASC_DENSE_TEST_CHECK(test, original != ilp64);
  provider.integer_abi = asc::LapackIntegerAbi::kSuffixed64;
  ASC_DENSE_TEST_CHECK(
      test, ilp64 != MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 3, 0,
                                  provider));
  provider.integer_abi = asc::LapackIntegerAbi::kLp64;
  const std::array<asc::extent_t, 1> too_large{0x80000000LL};
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackPlanIdentity::Create(
                 "dgetrf", asc::LapackScalarKind::kF64, too_large, {}, provider)
                 .ok());
}

void TestQueryRounding(TestContext& test) {
  using asc::LapackIntegerAbi;
  const auto zero =
      asc::CheckedLapackQueryEntries(0.0, LapackIntegerAbi::kLp64, 8);
  const auto fraction =
      asc::CheckedLapackQueryEntries(3.25, LapackIntegerAbi::kLp64, 8);
  ASC_DENSE_TEST_EQ(test, *zero, 0);
  ASC_DENSE_TEST_EQ(test, *fraction, 4);
  ASC_DENSE_TEST_CHECK(test, asc::CheckedLapackQueryEntries(
                                 2147483647.0, LapackIntegerAbi::kLp64, 1)
                                 .ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::CheckedLapackQueryEntries(0x1p31, LapackIntegerAbi::kLp64, 1).ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::CheckedLapackQueryEntries(0x1p63, LapackIntegerAbi::kIlp64, 1)
                 .ok());
  const double below = std::nextafter(0x1p63, 0.0);
  ASC_DENSE_TEST_CHECK(
      test,
      asc::CheckedLapackQueryEntries(below, LapackIntegerAbi::kIlp64, 1).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::CheckedLapackQueryEntries(below, LapackIntegerAbi::kIlp64, 8).ok());
  for (double invalid : {-1.0, std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN()}) {
    ASC_DENSE_TEST_CHECK(test, !asc::CheckedLapackQueryEntries(
                                    invalid, LapackIntegerAbi::kLp64, 1)
                                    .ok());
  }
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::CheckedLapackQueryEntries(1, LapackIntegerAbi::kLp64, 0).ok());
  const float float_boundary =
      std::nextafter(0x1p24F, std::numeric_limits<float>::infinity());
  ASC_DENSE_TEST_EQ(test,
                    *asc::CheckedLapackQueryEntries(float_boundary,
                                                    LapackIntegerAbi::kLp64, 4),
                    16777218);
}

void TestWorkspace(TestContext& test) {
  const auto identity = MakeIdentity();
  asc::LapackWorkspacePlan plan{.identity = identity};
  plan.regions[0] = {.minimum_entries = 2,
                     .preferred_entries = 4,
                     .entry_bytes = sizeof(double),
                     .alignment = alignof(double)};
  std::array<double, 8> storage{1, 2, 3, 4, 5, 6, 7, 8};
  const auto original = storage;
  asc::LapackWorkspace workspace;
  workspace.regions[0] = {storage.data(), 4 * sizeof(double),
                          asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test, asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  ASC_DENSE_TEST_EQ(
      test,
      asc::ValidateLapackWorkspace(plan, MakeIdentity("dgeqrf"), workspace, {})
          .code(),
      asc::ErrorCode::kInvalidState);
  workspace.regions[1] = {storage.data() + 2, 2 * sizeof(double),
                          asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  workspace.regions[1] = {storage.data() + 4, 2 * sizeof(double),
                          asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test, asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  std::array<asc::ConstMemoryView, 1> forbidden{asc::ConstMemoryView(
      storage.data() + 1, sizeof(double), asc::MemorySpace::kHost)};
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::ValidateLapackWorkspace(plan, identity, workspace, forbidden).ok());
  workspace.regions[0] = {storage.data(), sizeof(double),
                          asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  workspace.regions[0] = {storage.data(), 4 * sizeof(double),
                          asc::MemorySpace::kDevice};
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  workspace.regions[0] = {storage.data(), 4 * sizeof(double),
                          asc::MemorySpace::kHost};
  plan.total_byte_limit = 5 * sizeof(double);
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  plan.total_byte_limit = std::numeric_limits<std::size_t>::max();
  plan.regions[0].preferred_entries = std::numeric_limits<asc::extent_t>::max();
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  plan.regions[0].preferred_entries = 4;
  plan.regions[0].alignment = 3;
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  plan.regions[0].alignment = alignof(double);
  auto* unaligned = reinterpret_cast<std::byte*>(storage.data()) + 1;
  workspace.regions[0] = {unaligned, 4 * sizeof(double),
                          asc::MemorySpace::kHost};
  ASC_DENSE_TEST_CHECK(
      test, !asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  ASC_DENSE_TEST_EQ(test, storage, original);
}

void TestReportsAndAllocations(TestContext& test) {
  const auto identity = MakeIdentity();
  asc::LapackWorkspacePlan plan{.identity = identity};
  asc::LapackWorkspace workspace;
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = -4;
  report.native_argument = 4;
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  report.outcome = asc::LapackOutcome::kProviderArgument;
  report.output_validity = asc::LapackOutputValidity::kUnusable;
  std::size_t allocations = 0;
  {
    asc_dense_test::AllocationProbe probe;
    asc::InitializeLapackReport(identity, report);
    ASC_DENSE_TEST_CHECK(
        test, asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
    ASC_DENSE_TEST_CHECK(test, !asc::CheckedLapackQueryEntries(
                                    -1, asc::LapackIntegerAbi::kLp64, 1)
                                    .ok());
    allocations = probe.count();
  }
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info &&
                                 !report.native_argument &&
                                 !report.factor_family);
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, std::string_view(report.routine.data()), "dgetrf");
}

void TestWorkspaceIntegerAbi(TestContext& test) {
  asc::LapackProviderIdentity provider;
  provider.kind = asc::LapackProviderKind::kReference;
  provider.integer_abi = asc::LapackIntegerAbi::kLp64;
  provider.logical_bytes = 4;
  provider.source_sha256[0] = std::byte{1};
  provider.build_sha256[0] = std::byte{2};
  const auto identity =
      MakeIdentity("dgetrf", asc::LapackScalarKind::kF64, 3, 0, provider);
  asc::LapackWorkspacePlan plan{.identity = identity};
  const auto integer =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kInteger);
  plan.regions[integer] = {.minimum_entries = 1,
                           .preferred_entries = 1,
                           .entry_bytes = 8,
                           .alignment = 8};
  std::array<std::int64_t, 2> scratch{};
  asc::LapackWorkspace workspace;
  workspace.regions[integer] = {scratch.data(), sizeof(scratch),
                                asc::MemorySpace::kHost};
  ASC_DENSE_TEST_EQ(
      test, asc::ValidateLapackWorkspace(plan, identity, workspace, {}).code(),
      asc::ErrorCode::kConfiguration);
  plan.regions[integer].entry_bytes = 4;
  ASC_DENSE_TEST_CHECK(
      test, asc::ValidateLapackWorkspace(plan, identity, workspace, {}).ok());
  plan.regions[integer].preferred_entries = 0x80000000LL;
  ASC_DENSE_TEST_EQ(
      test, asc::ValidateLapackWorkspace(plan, identity, workspace, {}).code(),
      asc::ErrorCode::kOverflow);
}

void TestLayoutWorkspaceSizeDomain(TestContext& test) {
  asc::LapackProviderIdentity provider;
  provider.kind = asc::LapackProviderKind::kReference;
  provider.integer_abi = asc::LapackIntegerAbi::kLp64;
  provider.logical_bytes = 4;
  provider.source_sha256[0] = std::byte{1};
  provider.build_sha256[0] = std::byte{2};
  const auto identity =
      MakeIdentity("dgeequ", asc::LapackScalarKind::kF64, 3, 0, provider);
  const auto layout =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLayoutConversion);
  asc::LapackWorkspacePlan plan{.identity = identity};
  // Synthetic capacity metadata only. Zero mandatory entries require no huge
  // allocation or invented backing span; this is not a foreign numerical call.
  plan.regions[layout] = {.minimum_entries = 0,
                          .preferred_entries = 0x80000000LL,
                          .entry_bytes = 1,
                          .alignment = 1};
  const asc::LapackWorkspace workspace;
  const asc_dense_test::AllocationProbe probe;
  const auto optional =
      asc::ValidateLapackWorkspace(plan, identity, workspace, {});
  plan.regions[layout].minimum_entries = 0x80000000LL;
  const auto missing =
      asc::ValidateLapackWorkspace(plan, identity, workspace, {});
  plan.regions[layout].minimum_entries = 0;
  plan.regions[layout].preferred_entries =
      std::numeric_limits<asc::extent_t>::max();
  plan.regions[layout].entry_bytes = 16;
  const auto byte_overflow =
      asc::ValidateLapackWorkspace(plan, identity, workspace, {});
  const auto allocations = probe.count();
  ASC_DENSE_TEST_CHECK(test, optional.ok());
  ASC_DENSE_TEST_EQ(test, missing.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, byte_overflow.code(), asc::ErrorCode::kOverflow);
  ASC_DENSE_TEST_CHECK(test,
                       asc_test::ProcessAllocationCountMatches(allocations, 0));
}

void TestPivotsAndFactors(TestContext& test) {
  std::array<asc::index_t, 3> entries{3, 3, 3};
  auto pivots = asc::RawLapackPivotView::Create(
      entries.data(), 3, asc::LapackFactorFamily::kLuPartialPivot,
      {entries.data(), sizeof(entries), asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, pivots.ok());
  std::array<asc::index_t, 3> swaps{-9, -9, -9};
  ASC_DENSE_TEST_CHECK(
      test, asc::ConvertLuPivotsToZeroBasedSwaps(*pivots, 3, swaps).ok());
  const std::array<asc::index_t, 3> expected{2, 2, 2};
  ASC_DENSE_TEST_EQ(test, swaps, expected);
  ASC_DENSE_TEST_CHECK(
      test, !asc::ConvertLuPivotsToZeroBasedSwaps(*pivots, 3, entries).ok());
  entries[2] = 1;
  swaps.fill(-9);
  ASC_DENSE_TEST_CHECK(
      test, !asc::ConvertLuPivotsToZeroBasedSwaps(*pivots, 3, swaps).ok());
  const std::array<asc::index_t, 3> unchanged{-9, -9, -9};
  ASC_DENSE_TEST_EQ(test, swaps, unchanged);
  entries = {-2, -2, 3};
  auto blocks = asc::RawLapackPivotView::Create(
      entries.data(), 3, asc::LapackFactorFamily::kBunchKaufman,
      {entries.data(), sizeof(entries), asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, blocks.ok());
  ASC_DENSE_TEST_EQ(test, blocks->values()[0], -2);
  ASC_DENSE_TEST_CHECK(
      test, !asc::ConvertLuPivotsToZeroBasedSwaps(*blocks, 3, swaps).ok());
  entries = {3, 3, 3};
  std::array<double, 9> packed{};
  auto matrix = asc::DenseBlasMatrixView<const double>::Create(
      packed.data(), 3, 3, asc::DenseBlasLayout::kColumnMajor, 3,
      {packed.data(), sizeof(packed), asc::MemorySpace::kHost});
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::LapackLuFactorView<double>::Create(*matrix, *pivots, report).ok());
  report.outcome = asc::LapackOutcome::kSuccess;
  report.output_validity = asc::LapackOutputValidity::kComplete;
  report.factor_family = asc::LapackFactorFamily::kCholesky;
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::LapackLuFactorView<double>::Create(*matrix, *pivots, report).ok());
  report.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
  ASC_DENSE_TEST_CHECK(
      test,
      asc::LapackLuFactorView<double>::Create(*matrix, *pivots, report).ok());
  report.outcome = asc::LapackOutcome::kSingular;
  report.output_validity = asc::LapackOutputValidity::kDocumentedPartial;
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::LapackLuFactorView<double>::Create(*matrix, *pivots, report).ok());
}

void TestColumnPermutations(TestContext& test) {
  std::array<asc::index_t, 4> entries{3, 1, 4, 2};
  const auto raw = asc::RawLapackPivotView::Create(
      entries.data(), 4, asc::LapackFactorFamily::kColumnPivotedQr,
      {entries.data(), sizeof(entries), asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, raw.ok());
  if (!raw.ok()) {
    return;
  }
  std::array<asc::index_t, 4> output{-9, -9, -9, -9};
  std::array<std::byte, 6> scratch{};
  scratch.fill(std::byte{7});
  asc_dense_test::AllocationProbe probe;
  ASC_DENSE_TEST_CHECK(test,
                       asc::ValidateColumnPermutation(*raw, scratch).ok());
  ASC_DENSE_TEST_CHECK(
      test,
      asc::ConvertColumnPermutationToZeroBased(*raw, output, scratch).ok());
  ASC_DENSE_TEST_EQ(test, output, (std::array<asc::index_t, 4>{2, 0, 3, 1}));
  ASC_DENSE_TEST_EQ(test, scratch[4], std::byte{7});
  ASC_DENSE_TEST_EQ(test, scratch[5], std::byte{7});
  ASC_DENSE_TEST_CHECK(test, !asc::ValidateLuPivots(*raw, 4).ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::ConvertLuPivotsToZeroBasedSwaps(*raw, 4, output).ok());
  output.fill(-9);
  for (const asc::index_t invalid :
       {asc::index_t{0}, asc::index_t{5}, asc::index_t{3}}) {
    entries[2] = invalid;
    ASC_DENSE_TEST_CHECK(
        test,
        !asc::ConvertColumnPermutationToZeroBased(*raw, output, scratch).ok());
    ASC_DENSE_TEST_EQ(test, output,
                      (std::array<asc::index_t, 4>{-9, -9, -9, -9}));
  }
  entries[2] = 4;
  ASC_DENSE_TEST_CHECK(test, !asc::ConvertColumnPermutationToZeroBased(
                                  *raw, output, std::span(scratch).first(3))
                                  .ok());
  ASC_DENSE_TEST_CHECK(
      test,
      !asc::ConvertColumnPermutationToZeroBased(*raw, entries, scratch).ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::ConvertColumnPermutationToZeroBased(
                 *raw, output, std::as_writable_bytes(std::span(entries)))
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::ConvertColumnPermutationToZeroBased(
                 *raw, output, std::as_writable_bytes(std::span(output)))
                 .ok());
  ASC_DENSE_TEST_EQ(test, output,
                    (std::array<asc::index_t, 4>{-9, -9, -9, -9}));
  for (const auto family :
       {asc::LapackFactorFamily::kLuPartialPivot,
        asc::LapackFactorFamily::kBunchKaufman, asc::LapackFactorFamily::kRook,
        asc::LapackFactorFamily::kAasen}) {
    const auto other = asc::RawLapackPivotView::Create(
        entries.data(), 4, family,
        {entries.data(), sizeof(entries), asc::MemorySpace::kHost});
    ASC_DENSE_TEST_CHECK(test, other.ok());
    ASC_DENSE_TEST_CHECK(test,
                         !asc::ValidateColumnPermutation(*other, scratch).ok());
  }
  const auto empty = asc::RawLapackPivotView::Create(
      nullptr, 0, asc::LapackFactorFamily::kColumnPivotedQr,
      {nullptr, 0, asc::MemorySpace::kHost});
  ASC_DENSE_TEST_CHECK(test, empty.ok());
  ASC_DENSE_TEST_CHECK(
      test, asc::ConvertColumnPermutationToZeroBased(*empty, {}, {}).ok());
  ASC_DENSE_TEST_CHECK(
      test, asc_test::ProcessAllocationCountMatches(probe.count(), 0));
}

}  // namespace

int main() {
  TestContext test;
  TestIdentity(test);
  TestProviderIdentity(test);
  TestQueryRounding(test);
  TestWorkspace(test);
  TestReportsAndAllocations(test);
  TestWorkspaceIntegerAbi(test);
  TestLayoutWorkspaceSizeDomain(test);
  TestPivotsAndFactors(test);
  TestColumnPermutations(test);
  return test.Finish();
}
