#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_band.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_factor_test_support.h"
#include "lu_band_faults.h"
#include "lu_band_test_support.h"

namespace {
namespace support = asc_lu_band_test;
namespace faults = asc_lu_band_faults;
using faults::Fault;
using support::Take;
using support::TestContext;
using support::WithoutAllocation;

std::int64_t Minimum(const asc::LapackWorkspacePlan& plan) {
  return plan.regions[support::kInteger].entry_bytes == 4
             ? std::numeric_limits<std::int32_t>::min()
             : std::numeric_limits<std::int64_t>::min();
}

std::int64_t ExpectedInfo(Fault fault, const asc::LapackWorkspacePlan& plan,
                          std::int64_t positive, std::int64_t ordinary) {
  if (fault == Fault::kNegative) {
    return -4;
  }
  if (fault == Fault::kMinimum || fault == Fault::kUnwrittenInfo) {
    return Minimum(plan);
  }
  return fault == Fault::kLargePositive ? positive : ordinary;
}

template <typename T>
void FactorFaults(TestContext& test,
                  const asc::ReferenceLapackProvider& provider) {
  for (const auto fault :
       {Fault::kNegative, Fault::kMinimum, Fault::kLargePositive,
        Fault::kZeroPivot, Fault::kNegativePivot, Fault::kOutOfBandPivot,
        Fault::kLatePivot, Fault::kPartialWidth, Fault::kUnwrittenInfo}) {
    support::Band<T> band(5, 5, 1, 2);
    support::Pivots pivots(5);
    const auto before = band.values;
    const auto pivot_before = pivots.values;
    const auto matrix = band.View();
    const auto swaps = pivots.View();
    const auto plan =
        Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
    support::Scratch<T> scratch(plan);
    const auto workspace = scratch.View();
    asc::LapackReport report;
    faults::Select(fault);
    const auto status = WithoutAllocation(test, [&] {
      return asc_lu_band_test::Factor(provider, matrix, swaps, plan, workspace,
                                      report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    const auto expected_info = ExpectedInfo(fault, plan, 6, 0);
    ASC_DENSE_TEST_EQ(test, report.native_info, expected_info);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnusable);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      expected_info < 0 ? asc::LapackOutcome::kProviderArgument
                                        : asc::LapackOutcome::kPartialResult);
    ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value() &&
                                   !report.diagnostic_index.has_value());
    ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                      fault == Fault::kNegative);
    if (fault == Fault::kNegative) {
      ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
    }
    ASC_DENSE_TEST_EQ(test, pivots.values, pivot_before);
    ASC_DENSE_TEST_EQ(test, band.values[band.Index(0, 0)], T{123});
    band.Padding(test, before);
    scratch.Check(test);
    const auto rejected = asc::ReferenceLuBandFactorView<T>::Create(
        provider, band.ConstView(), pivots.ConstView(), report);
    ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidState);
  }
}

template <typename T>
void SolveFaults(TestContext& test,
                 const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  faults::Select(Fault::kPass);
  ASC_DENSE_TEST_CHECK(test, asc_lu_band_test::Factor(provider, matrix, swaps,
                                                      plan, workspace, report)
                                 .ok());
  const auto factor = Take(asc::ReferenceLuBandFactorView<T>::Create(
      provider, band.ConstView(), pivots.ConstView(), report));
  const auto factored = band.values;
  const auto pivot_before = pivots.values;
  for (const auto layout : {support::kColumn, support::kRow}) {
    for (const auto operation : support::kOperations) {
      for (const auto fault : {Fault::kNegative, Fault::kMinimum,
                               Fault::kLargePositive, Fault::kUnwrittenInfo}) {
        support::Rhs<T> rhs(band, 2, layout, operation);
        const auto before = rhs.values;
        const auto destination = rhs.View();
        const auto solve_plan = Take(
            asc::QueryGbtrsWorkspace(provider, operation, factor, destination));
        support::Scratch<T> solve_scratch(solve_plan);
        const auto solve_workspace = solve_scratch.View();
        faults::Select(fault);
        asc::LapackReport solve_report;
        const auto status = WithoutAllocation(test, [&] {
          return asc::Gbtrs(provider, operation, factor, destination,
                            solve_plan, solve_workspace, solve_report);
        });
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
        ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
        ASC_DENSE_TEST_CHECK(test, solve_report.called_provider);
        const auto info = ExpectedInfo(fault, solve_plan, 1, 1);
        ASC_DENSE_TEST_EQ(test, solve_report.native_info, info);
        ASC_DENSE_TEST_EQ(test, solve_report.output_validity,
                          asc::LapackOutputValidity::kUnusable);
        ASC_DENSE_TEST_EQ(test, solve_report.native_argument.has_value(),
                          fault == Fault::kNegative);
        auto expected = before;
        if (layout == support::kColumn) {
          expected[rhs.Index(0, 0)] = T{123};
        }
        ASC_DENSE_TEST_CHECK(test, support::SameBytes(rhs.values, expected));
        ASC_DENSE_TEST_CHECK(test, support::SameBytes(band.values, factored));
        ASC_DENSE_TEST_EQ(test, pivots.values, pivot_before);
        solve_scratch.Check(test);
      }
    }
  }
}

template <typename T>
void PlanRejections(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto before = band.values;
  const auto pivot_before = pivots.values;
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  // Every malformed plan/workspace is checked by the real wrapper under
  // native-call interception, with byte-identical operands and no allocations.
  for (int which = 0; which < 9; ++which) {
    auto bad_plan = plan;
    auto bad_workspace = workspace;
    if (which == 0) {
      bad_plan.regions[support::kInteger].minimum_entries -= 1;
    } else if (which == 1) {
      bad_plan.total_byte_limit -= 1;
    } else if (which == 2) {
      bad_workspace.regions[support::kInteger] = {nullptr, 0, support::kHost};
    } else if (which == 3) {
      const auto region = workspace.regions[support::kInteger];
      bad_workspace.regions[support::kInteger] = {
          static_cast<std::byte*>(region.data()) + 1, region.size() - 1,
          support::kHost};
    } else if (which == 4) {
      bad_workspace.regions[support::kInteger] = {band.values.data() + 1,
                                                  sizeof(T), support::kHost};
    } else if (which == 5) {
      bad_workspace.regions[support::kLayout] =
          workspace.regions[support::kInteger];
    } else if (which == 6) {
      const auto region = workspace.regions[support::kInteger];
      bad_workspace.regions[support::kInteger] = {
          region.data(), region.size(), asc::MemorySpace::kPinnedHost};
    } else if (which == 7) {
      bad_workspace.regions[support::kLayout] = {
          scratch.layout.data(), sizeof(T), asc::MemorySpace::kPinnedHost};
    } else {
      bad_plan.identity = Take(asc::LapackPlanIdentity::Create(
          "sgbtrs", asc::LapackScalarKind::kF32, std::array<asc::extent_t, 0>{},
          std::array<std::int64_t, 0>{}, provider.identity()));
    }
    faults::Select(Fault::kLargePositive);
    asc::LapackReport rejected;
    const auto status = WithoutAllocation(test, [&] {
      return asc_lu_band_test::Factor(provider, matrix, swaps, bad_plan,
                                      bad_workspace, rejected);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    ASC_DENSE_TEST_CHECK(
        test, !rejected.called_provider && !rejected.native_info.has_value());
    ASC_DENSE_TEST_EQ(test, rejected.outcome, asc::LapackOutcome::kNotRun);
    ASC_DENSE_TEST_EQ(test, rejected.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(before, band.values));
    ASC_DENSE_TEST_EQ(test, pivot_before, pivots.values);
    scratch.Check(test);
  }
}

template <typename T>
void FactorMetadataRejections(TestContext& test,
                              const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  faults::Select(Fault::kPass);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc_lu_band_test::Factor(provider, matrix, swaps,
                                                      plan, workspace, report)
                                 .ok());
  // Reject every unsupported outcome and provenance mutation; no labels are
  // inferred to mean success. Matrix coefficients are not scanned by Create.
  for (int which = 0; which < 11; ++which) {
    auto bad = report;
    if (which == 0) {
      bad.outcome = asc::LapackOutcome::kAccuracyWarning;
    } else if (which == 1) {
      bad.outcome = asc::LapackOutcome::kPartialResult;
    } else if (which == 2) {
      bad.output_validity = asc::LapackOutputValidity::kDocumentedPartial;
    } else if (which == 3) {
      bad.native_info = 1;
    } else if (which == 4) {
      bad.native_info.reset();
    } else if (which == 5) {
      bad.called_provider = false;
    } else if (which == 6) {
      bad.routine[1] = 'x';
    } else if (which == 7) {
      bad.factor_family = asc::LapackFactorFamily::kLuPartialPivot;
    } else if (which == 8) {
      bad.native_argument = 1;
    } else if (which == 9) {
      bad.diagnostic_index = 0;
    } else {
      bad.provider = {};
    }
    const auto rejected = WithoutAllocation(test, [&] {
      return asc::ReferenceLuBandFactorView<T>::Create(
          provider, band.ConstView(), pivots.ConstView(), bad);
    });
    ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidState);
  }
  for (const auto value : {asc::index_t{0}, asc::index_t{-1}, asc::index_t{3},
                           std::numeric_limits<asc::index_t>::min(),
                           std::numeric_limits<asc::index_t>::max()}) {
    const auto saved = pivots.values[1];
    pivots.values[1] = value;
    const auto rejected = WithoutAllocation(test, [&] {
      return asc::ReferenceLuBandFactorView<T>::Create(
          provider, band.ConstView(), pivots.ConstView(), report);
    });
    ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidArgument);
    pivots.values[1] = saved;
  }
}

template <typename T>
void EveryPivotRejection(TestContext& test,
                         const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  faults::Select(Fault::kPass);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc_lu_band_test::Factor(provider, matrix, swaps,
                                                      plan, workspace, report)
                                 .ok());
  // The factory checks the entire encoding, including the last swap.
  for (std::size_t i = 1; i <= 5; ++i) {
    const auto saved = pivots.values[i];
    pivots.values[i] = 0;
    const auto rejected = WithoutAllocation(test, [&] {
      return asc::ReferenceLuBandFactorView<T>::Create(
          provider, band.ConstView(), pivots.ConstView(), report);
    });
    ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                      asc::ErrorCode::kInvalidArgument);
    pivots.values[i] = saved;
  }
}

template <typename T>
void SolvePreflight(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  faults::Select(Fault::kPass);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc_lu_band_test::Factor(provider, matrix, swaps,
                                                      plan, workspace, report)
                                 .ok());
  const auto factor = Take(asc::ReferenceLuBandFactorView<T>::Create(
      provider, band.ConstView(), pivots.ConstView(), report));
  for (const auto layout : {support::kColumn, support::kRow}) {
    for (const auto operation : support::kOperations) {
      support::Rhs<T> rhs(band, 2, layout, operation);
      const auto destination = rhs.View();
      const auto solve_plan = Take(
          asc::QueryGbtrsWorkspace(provider, operation, factor, destination));
      support::Scratch<T> solve_scratch(solve_plan);
      const auto solve_workspace = solve_scratch.View();
      const auto rhs_before = rhs.values;
      for (int which = 0; which < 5; ++which) {
        auto mode = operation;
        auto bad_plan = solve_plan;
        auto bad_workspace = solve_workspace;
        const auto saved_pivot = pivots.values[5];
        const auto diagonal = band.values[band.Index(4, 4)];
        if (which == 0) {
          // Deliberately invalid fixed-underlying enum exercises preflight.
          // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
          mode = static_cast<asc::DenseBlasTranspose>(99);
        } else if (which == 1) {
          bad_plan = plan;
        } else if (which == 2) {
          bad_workspace.regions[support::kInteger] = {nullptr, 0,
                                                      support::kHost};
        } else if (which == 3) {
          pivots.values[5] = -1;
        } else {
          band.values[band.Index(4, 4)] = T{};
        }
        faults::Select(Fault::kLargePositive);
        asc::LapackReport rejected;
        const auto status = WithoutAllocation(test, [&] {
          return asc::Gbtrs(provider, mode, factor, destination, bad_plan,
                            bad_workspace, rejected);
        });
        ASC_DENSE_TEST_CHECK(test, !status.ok());
        ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
        ASC_DENSE_TEST_CHECK(test, !rejected.called_provider &&
                                       !rejected.native_info.has_value());
        ASC_DENSE_TEST_EQ(test, rejected.output_validity,
                          asc::LapackOutputValidity::kUnchanged);
        ASC_DENSE_TEST_CHECK(test, support::SameBytes(rhs_before, rhs.values));
        if (which == 4) {
          ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
          ASC_DENSE_TEST_EQ(test, rejected.diagnostic_index, 4);
        }
        pivots.values[5] = saved_pivot;
        band.values[band.Index(4, 4)] = diagonal;
        solve_scratch.Check(test);
      }
    }
  }
}

template <typename T>
void DescriptorFactorRejections(TestContext& test,
                                const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  const auto before = band.values;
  const auto pivots_before = pivots.values;
  for (int which = 0; which < 4; ++which) {
    auto rejected_matrix = matrix;
    auto rejected_swaps = swaps;
    if (which == 0) {
      rejected_matrix = band.View(asc::MemorySpace::kPinnedHost);
    } else if (which == 1) {
      rejected_swaps = pivots.View(asc::MemorySpace::kPinnedHost);
    } else if (which == 2) {
      rejected_swaps = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
          pivots.values.data() + 1, 4, 1,
          {pivots.values.data(), pivots.values.size() * sizeof(asc::index_t),
           support::kHost}));
    } else {
      rejected_matrix = Take(asc::LapackLuBandView<T>::Create(
          matrix.storage().data(), 5, 5, 0, 2, band.ld,
          {band.values.data(), band.values.size() * sizeof(T),
           support::kHost}));
    }
    asc::LapackReport report;
    faults::Select(Fault::kLargePositive);
    const auto status = WithoutAllocation(test, [&] {
      return asc_lu_band_test::Factor(provider, rejected_matrix, rejected_swaps,
                                      plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    ASC_DENSE_TEST_CHECK(
        test, !report.called_provider && !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(before, band.values));
    ASC_DENSE_TEST_EQ(test, pivots_before, pivots.values);
  }
}

template <typename T>
void DescriptorFactories(TestContext& test,
                         const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  // A report used as a byte workspace must be rejected without resetting the
  // live report object. There is no fabricated numerical object in this test.
  asc::LapackReport alias_report;
  alias_report.called_provider = true;
  alias_report.native_info = 79;
  const auto alias_before = alias_report;
  auto alias_workspace = workspace;
  alias_workspace.regions[support::kInteger] = {
      &alias_report, sizeof(alias_report), support::kHost};
  faults::Select(Fault::kLargePositive);
  ASC_DENSE_TEST_EQ(test,
                    asc_lu_band_test::Factor(provider, matrix, swaps, plan,
                                             alias_workspace, alias_report)
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
  ASC_DENSE_TEST_EQ(test, alias_report.called_provider,
                    alias_before.called_provider);
  ASC_DENSE_TEST_EQ(test, alias_report.native_info, alias_before.native_info);
  ASC_DENSE_TEST_EQ(test, alias_report.routine, alias_before.routine);
  ASC_DENSE_TEST_EQ(test, alias_report.provider, alias_before.provider);
  // Malformed descriptor factories use real finite backing and are separate
  // from pure INTEGER limit tests, avoiding fictitious huge allocation spans.
  const asc::ConstMemoryView backing{
      band.values.data(), band.values.size() * sizeof(T), support::kHost};
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuBandView<T>::Create(matrix.storage().data(), -1, 5, 1,
                                              2, band.ld, backing)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuBandView<T>::Create(matrix.storage().data(), 5, 5, -1,
                                              2, band.ld, backing)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuBandView<T>::Create(matrix.storage().data(), 5, 5, 1,
                                              2, 4, backing)
                 .ok());
  ASC_DENSE_TEST_CHECK(
      test, !asc::LapackLuBandView<T>::Create(
                 matrix.storage().data(), 5, 5, 1, 2, band.ld,
                 {matrix.storage().data(), sizeof(T), support::kHost})
                 .ok());
}

template <typename T>
void SolveDescriptorRejections(TestContext& test,
                               const asc::ReferenceLapackProvider& provider) {
  support::Band<T> band(5, 5, 1, 2);
  support::Pivots pivots(5);
  const auto matrix = band.View();
  const auto swaps = pivots.View();
  const auto plan =
      Take(asc_lu_band_test::QueryFactor(provider, matrix, swaps));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  faults::Select(Fault::kPass);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, asc_lu_band_test::Factor(provider, matrix, swaps,
                                                      plan, workspace, report)
                                 .ok());
  const auto factor = Take(asc::ReferenceLuBandFactorView<T>::Create(
      provider, band.ConstView(), pivots.ConstView(), report));
  const auto factored = band.values;
  const asc::ConstMemoryView backing{
      band.values.data(), band.values.size() * sizeof(T), support::kHost};
  support::Rhs<T> rhs(band, 1, support::kColumn,
                      asc::DenseBlasTranspose::kNone);
  const auto rhs_before = rhs.values;
  // A one-column ASC leading dimension is never advanced; preserve it in the
  // plan identity, but use the equivalent finite N stride at the ABI boundary.
  const auto one_column = Take(asc::DenseBlasMatrixView<T>::Create(
      rhs.values.data() + 1, 5, 1, support::kColumn,
      std::numeric_limits<asc::stride_t>::max(),
      {rhs.values.data(), rhs.values.size() * sizeof(T), support::kHost}));
  const auto solve_plan = Take(asc::QueryGbtrsWorkspace(
      provider, asc::DenseBlasTranspose::kNone, factor, one_column));
  support::Scratch<T> solve_scratch(solve_plan);
  const auto solve_workspace = solve_scratch.View();
  asc::LapackReport solve_report;
  faults::Select(Fault::kPass);
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return asc::Gbtrs(provider,
                                                 asc::DenseBlasTranspose::kNone,
                                                 factor, one_column, solve_plan,
                                                 solve_workspace, solve_report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  rhs.Check(test, band, rhs_before);
  solve_scratch.Check(test);
  rhs.values = rhs_before;
  for (int which = 0; which < 3; ++which) {
    auto bad_rhs = rhs.View();
    if (which == 0) {
      bad_rhs = rhs.View(asc::MemorySpace::kPinnedHost);
    } else if (which == 1) {
      bad_rhs = Take(asc::DenseBlasMatrixView<T>::Create(
          rhs.values.data() + 1, 4, 1, support::kColumn, 5,
          {rhs.values.data(), rhs.values.size() * sizeof(T), support::kHost}));
    } else {
      bad_rhs = Take(asc::DenseBlasMatrixView<T>::Create(
          matrix.storage().data(), 5, 1, support::kColumn, 5, backing));
    }
    faults::Select(Fault::kLargePositive);
    asc::LapackReport rejected;
    const auto status = WithoutAllocation(test, [&] {
      return asc::Gbtrs(provider, asc::DenseBlasTranspose::kNone, factor,
                        bad_rhs, solve_plan, solve_workspace, rejected);
    });
    ASC_DENSE_TEST_CHECK(test, !status.ok());
    ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
    ASC_DENSE_TEST_CHECK(
        test, !rejected.called_provider && !rejected.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(rhs_before, rhs.values));
    ASC_DENSE_TEST_CHECK(test, support::SameBytes(factored, band.values));
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  FactorFaults<T>(test, provider);
  SolveFaults<T>(test, provider);
  PlanRejections<T>(test, provider);
  FactorMetadataRejections<T>(test, provider);
  EveryPivotRejection<T>(test, provider);
  SolvePreflight<T>(test, provider);
  DescriptorFactorRejections<T>(test, provider);
  DescriptorFactories<T>(test, provider);
  SolveDescriptorRejections<T>(test, provider);
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (!asc_lu_band_test::SelectFactor(argc, argv)) {
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
