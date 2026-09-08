#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_band_condition.h"
#include "asc/dense/providers/lapack_lu_band_driver.h"
#include "asc/dense/providers/lapack_lu_band_equilibration.h"
#include "asc/dense/providers/lapack_lu_band_refinement.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "installed_lu/normal_return_guard.h"
#include "lu_band_driver_test_support.h"
#include "lu_band_expert_faults.h"
#include "lu_band_expert_test_support.h"
#include "lu_band_test_support.h"
namespace {
namespace support = asc_lu_band_expert_test;
namespace base = asc_lu_band_test;
namespace driver = asc_lu_band_driver_test;
namespace faults = asc_lu_band_expert_faults;
using faults::Fault;
using support::Take;
using support::TestContext;
constexpr std::array kRoutines{
    faults::Routine::kEquilibrate, faults::Routine::kCondition,
    faults::Routine::kRefine,      faults::Routine::kSolve,
    faults::Routine::kDriver,      faults::Routine::kDriver,
    faults::Routine::kDriver};
template <typename T>
base::Band<T> Initial(bool cold) {
  base::Band<T> band(3, 3, 1, 1);
  if (cold) {
    // Exact pinned GBTRF identity fixture: U=I, multipliers=0, pivots=1,2,3.
    for (asc::extent_t j = 0; j < 3; ++j) {
      for (asc::extent_t i = 0; i < 3; ++i) {
        if (i >= j - 2 && i <= j + 1) {
          band.Put(i, j, i == j ? T{1} : T{});
        }
      }
    }
  }
  return band;
}
template <typename T>
struct Case {
  using Real = asc::DenseBlasRealType<T>;
  driver::Sample<T> data;
  asc::LapackEquilibrationStatistics<Real> equilibration{Real{-281}, Real{-283},
                                                         Real{-293}};
  Real rcond = Real{-307};
  Real original_norm;
  int route;
  Case(TestContext& test, const asc::ReferenceLapackProvider& provider,
       int selected, bool cold = false)
      : data(Initial<T>(cold), 3, asc::DenseBlasTranspose::kConjugateTranspose,
             base::kRow, base::kRow),
        original_norm(cold ? Real{1} : Real{8}),
        route(selected) {
    faults::Select(kRoutines[static_cast<std::size_t>(route)], Fault::kPass);
    if (route == 1 || route == 2 || route == 6) {
      if (cold) {
        for (asc::index_t i = 0; i < 3; ++i) {
          data.pivots.values[static_cast<std::size_t>(i + 1)] = i + 1;
        }
        data.af.Reconstruct(test, data.pivots);
      } else {
        data.Supply(test, provider, asc::LapackEquilibration::kNone);
      }
    }
    if (route == 2) {
      for (asc::extent_t i = 0; i < 3; ++i) {
        for (asc::extent_t j = 0; j < 3; ++j) {
          const auto value =
              data.x.expected[static_cast<std::size_t>(3 * i + j)];
          data.x.values[data.x.Index(i, j)] =
              support::Value<T>(value.real() + 1.0L / 128, value.imag());
        }
      }
    }
  }
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if (route == 0) {
      return asc::QueryGbequWorkspace(provider, data.a.ConstView(),
                                      data.rows.View(), data.columns.View(),
                                      equilibration);
    }
    if (route == 1) {
      return asc::QueryGbconWorkspace(
          provider, asc::LapackConditionNorm::kOne, data.af.ConstView(),
          Take(asc::ReferenceLuBandPivotView::Create(data.pivots.ConstView())),
          original_norm, rcond);
    }
    if (route == 2) {
      return asc::QueryGbrfsWorkspace(
          provider, data.b.operation, data.a.ConstView(), data.af.ConstView(),
          Take(asc::ReferenceLuBandPivotView::Create(data.pivots.ConstView())),
          data.ConstB(), data.x.View(), data.ferr.View(), data.berr.View());
    }
    if (route == 3) {
      return asc::QueryGbsvWorkspace(provider, data.af.View(),
                                     data.pivots.View(), data.b.View());
    }
    return data.Query(provider, route - 4);
  }
  auto Execute(const asc::ReferenceLapackProvider& provider,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::LapackReport& report) {
    if (route == 0) {
      return asc::Gbequ(provider, data.a.ConstView(), data.rows.View(),
                        data.columns.View(), equilibration, plan, workspace,
                        report);
    }
    if (route == 1) {
      return asc::Gbcon(
          provider, asc::LapackConditionNorm::kOne, data.af.ConstView(),
          Take(asc::ReferenceLuBandPivotView::Create(data.pivots.ConstView())),
          original_norm, rcond, plan, workspace, report);
    }
    if (route == 2) {
      return asc::Gbrfs(
          provider, data.b.operation, data.a.ConstView(), data.af.ConstView(),
          Take(asc::ReferenceLuBandPivotView::Create(data.pivots.ConstView())),
          data.ConstB(), data.x.View(), data.ferr.View(), data.berr.View(),
          plan, workspace, report);
    }
    if (route == 3) {
      return asc::Gbsv(provider, data.af.View(), data.pivots.View(),
                       data.b.View(), plan, workspace, report);
    }
    return data.Execute(provider, route - 4, plan, workspace, report);
  }
};
template <typename T>
void Unchanged(TestContext& test, const Case<T>& after, const Case<T>& before) {
  ASC_DENSE_TEST_CHECK(
      test, support::SameBytes(after.data.a.values, before.data.a.values));
  ASC_DENSE_TEST_CHECK(
      test, support::SameBytes(after.data.af.values, before.data.af.values));
  ASC_DENSE_TEST_CHECK(
      test, support::SameBytes(after.data.b.values, before.data.b.values));
  ASC_DENSE_TEST_CHECK(
      test, support::SameBytes(after.data.x.values, before.data.x.values));
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(after.data.rows.values,
                                                before.data.rows.values));
  ASC_DENSE_TEST_CHECK(test, support::SameBytes(after.data.columns.values,
                                                before.data.columns.values));
  ASC_DENSE_TEST_EQ(test, after.data.pivots.values, before.data.pivots.values);
  ASC_DENSE_TEST_EQ(test, after.data.ferr.values, before.data.ferr.values);
  ASC_DENSE_TEST_EQ(test, after.data.berr.values, before.data.berr.values);
  ASC_DENSE_TEST_EQ(test, after.data.equed, before.data.equed);
  ASC_DENSE_TEST_EQ(test, after.data.stats.reciprocal_condition,
                    before.data.stats.reciprocal_condition);
  ASC_DENSE_TEST_EQ(test, after.data.stats.reciprocal_pivot_growth,
                    before.data.stats.reciprocal_pivot_growth);
  ASC_DENSE_TEST_EQ(test, after.equilibration.row_condition,
                    before.equilibration.row_condition);
  ASC_DENSE_TEST_EQ(test, after.equilibration.column_condition,
                    before.equilibration.column_condition);
  ASC_DENSE_TEST_EQ(test, after.equilibration.absolute_maximum,
                    before.equilibration.absolute_maximum);
  ASC_DENSE_TEST_EQ(test, after.rcond, before.rcond);
}
void SeedIntegerBytes(const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace, bool valid) {
  auto* bytes =
      static_cast<std::byte*>(workspace.regions[support::kInteger].data());
  const auto count = plan.regions[support::kInteger].minimum_entries;
  const auto width = plan.regions[support::kInteger].entry_bytes;
  for (asc::extent_t i = 0; i < count; ++i) {
    const std::int64_t value = valid ? i + 1 : 0;
    if (width == 4) {
      const auto narrow = static_cast<std::int32_t>(value);
      std::memcpy(bytes + static_cast<std::size_t>(i) * width, &narrow, width);
    } else {
      std::memcpy(bytes + static_cast<std::size_t>(i) * width, &value, width);
    }
  }
}
std::int64_t Info(const asc::ReferenceLapackProvider& provider, int route,
                  Fault fault) {
  if (fault == Fault::kNegative) {
    return -4;
  }
  if (fault == Fault::kMinimum || fault == Fault::kNoInfo ||
      fault == Fault::kPartialInfo) {
    return provider.identity().integer_abi == asc::LapackIntegerAbi::kLp64
               ? std::numeric_limits<std::int32_t>::min()
               : std::numeric_limits<std::int64_t>::min();
  }
  if (fault == Fault::kLargePositive) {
    return std::array{7, 1, 1, 4, 5, 5, 5}[static_cast<std::size_t>(route)];
  }
  return 0;
}
template <typename T>
void NativeFailure(TestContext& test,
                   const asc::ReferenceLapackProvider& provider, int route,
                   Fault fault, bool prior_valid) {
  Case<T> sample(test, provider, route);
  const auto before = sample;
  const auto plan = Take(sample.Query(provider));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  SeedIntegerBytes(plan, workspace, prior_valid);
  asc::LapackReport report;
  faults::Select(kRoutines[static_cast<std::size_t>(route)], fault);
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, plan, workspace, report); });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  ASC_DENSE_TEST_CHECK(test, faults::SawInfoSentinel());
  if (route >= 3 && route <= 5) {
    ASC_DENSE_TEST_CHECK(test, faults::SawPivotSentinels());
  }
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info, Info(provider, route, fault));
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnusable);
  ASC_DENSE_TEST_EQ(test, report.native_argument.has_value(),
                    fault == Fault::kNegative);
  if (fault == Fault::kNegative) {
    ASC_DENSE_TEST_EQ(test, report.native_argument, 4);
  }
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  Unchanged(test, sample, before);
  scratch.Check(test);
}
auto Bytes(const asc::LapackWorkspace& workspace) {
  std::vector<std::byte> result;
  for (const auto& region : workspace.regions) {
    if (region.size() != 0) {
      const auto* first = static_cast<const std::byte*>(region.data());
      result.insert(result.end(), first, first + region.size());
    }
  }
  return result;
}
template <typename T>
void Structural(TestContext& test, const asc::ReferenceLapackProvider& provider,
                int route) {
  Case<T> sample(test, provider, route);
  const auto before = sample;
  const auto plan = Take(sample.Query(provider));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  const auto bytes = Bytes(workspace);
  auto stale = plan;
  --stale.total_byte_limit;
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 777;
  faults::Select(kRoutines[static_cast<std::size_t>(route)], Fault::kNegative);
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, stale, workspace, report); });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidState);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  Unchanged(test, sample, before);
  ASC_DENSE_TEST_EQ(test, Bytes(workspace), bytes);
  auto aliased = workspace;
  auto alias_plan = plan;
  report.called_provider = true;
  report.native_info = 777;
  report.diagnostic_index = 2;
  asc::Status alias_status;
  if (route == 0) {
    // R/C remain live scalar arrays. A byte workspace region legitimately
    // names the plan's object representation; metadata rejection must happen
    // before resetting the report, even though GBEQU requires zero scratch.
    aliased.regions[support::kInteger] = {&alias_plan, sizeof(alias_plan),
                                          base::kHost};
    alias_status =
        asc::Gbequ(provider, sample.data.a.ConstView(), sample.data.rows.View(),
                   sample.data.columns.View(), sample.equilibration, alias_plan,
                   aliased, report);
  } else {
    for (auto& region : aliased.regions) {
      if (region.size() != 0) {
        ASC_DENSE_TEST_CHECK(test, region.size() <= sizeof(alias_plan));
        region = {&alias_plan, region.size(), base::kHost};
        break;
      }
    }
    alias_status = sample.Execute(provider, alias_plan, aliased, report);
  }
  ASC_DENSE_TEST_EQ(test, alias_status.code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
  ASC_DENSE_TEST_CHECK(test, report.called_provider &&
                                 report.native_info == 777 &&
                                 report.diagnostic_index == 2);
  Unchanged(test, sample, before);
  ASC_DENSE_TEST_EQ(test, Bytes(workspace), bytes);
  scratch.Check(test);
}

template <typename T>
void Rejection(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Case<T>& sample, const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace,
               asc::ErrorCode expected = asc::ErrorCode::kInvalidArgument) {
  const auto before = sample;
  const auto bytes = Bytes(workspace);
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 777;
  faults::Select(kRoutines[static_cast<std::size_t>(sample.route)],
                 Fault::kNegative);
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, plan, workspace, report); });
  ASC_DENSE_TEST_EQ(test, status.code(), expected);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 0);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  if (expected == asc::ErrorCode::kNumerical) {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSingular);
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 2);
  }
  Unchanged(test, sample, before);
  ASC_DENSE_TEST_EQ(test, Bytes(workspace), bytes);
}
template <typename T>
void WorkspaceFailures(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       int route) {
  Case<T> sample(test, provider, route);
  const auto plan = Take(sample.Query(provider));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  for (std::size_t region = 0; region < workspace.regions.size(); ++region) {
    const auto original = workspace.regions[region];
    if (original.size() == 0) {
      continue;
    }
    auto bad = workspace;
    bad.regions[region] = {original.data(), original.size() - 1, base::kHost};
    Rejection(test, provider, sample, plan, bad);
    bad.regions[region] = {static_cast<std::byte*>(original.data()) + 1,
                           original.size(), base::kHost};
    Rejection(test, provider, sample, plan, bad);
    bad.regions[region] = {original.data(), original.size(),
                           asc::MemorySpace::kDevice};
    Rejection(test, provider, sample, plan, bad, asc::ErrorCode::kMemoryAccess);
    const auto capacity = sample.data.af.values.size() * sizeof(T);
    ASC_DENSE_TEST_CHECK(test, capacity >= original.size());
    bad.regions[region] = {sample.data.af.values.data() + 1, original.size(),
                           base::kHost};
    Rejection(test, provider, sample, plan, bad);
  }
  scratch.Check(test);
}
template <typename T>
void InputFailures(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  using Real = asc::DenseBlasRealType<T>;
  for (const int route : {1, 2, 6}) {
    for (const asc::index_t invalid : {0, -1, 4}) {
      Case<T> sample(test, provider, route);
      sample.data.pivots.values[3] = invalid;
      const auto plan = Take(sample.Query(provider));
      support::Scratch<T> scratch(plan);
      Rejection(test, provider, sample, plan, scratch.View());
      scratch.Check(test);
    }
    Case<T> sample(test, provider, route);
    sample.data.pivots.values[1] = 3;  // In [1,n], outside source band [1,2].
    const auto plan = Take(sample.Query(provider));
    support::Scratch<T> scratch(plan);
    Rejection(test, provider, sample, plan, scratch.View());
    scratch.Check(test);
  }
  for (const int route : {2, 6}) {
    Case<T> sample(test, provider, route);
    sample.data.af.values[sample.data.af.Index(2, 2)] = T{};
    const auto plan = Take(sample.Query(provider));
    support::Scratch<T> scratch(plan);
    Rejection(test, provider, sample, plan, scratch.View(),
              asc::ErrorCode::kNumerical);
    scratch.Check(test);
  }
  for (const auto selected :
       {asc::LapackEquilibration::kRows, asc::LapackEquilibration::kColumns}) {
    for (const Real invalid :
         {Real{0}, Real{-1}, std::numeric_limits<Real>::infinity(),
          std::numeric_limits<Real>::quiet_NaN()}) {
      Case<T> sample(test, provider, 6);
      sample.data.Supply(test, provider, selected);
      auto& values = selected == asc::LapackEquilibration::kRows
                         ? sample.data.rows.values
                         : sample.data.columns.values;
      values[3] = invalid;
      const auto plan = Take(sample.Query(provider));
      support::Scratch<T> scratch(plan);
      Rejection(test, provider, sample, plan, scratch.View());
      scratch.Check(test);
    }
  }
}
template <typename T>
void Quality(TestContext& test, const asc::ReferenceLapackProvider& provider,
             int route, Fault fault) {
  Case<T> sample(test, provider, route);
  const auto before = sample;
  const auto plan = Take(sample.Query(provider));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  faults::Select(kRoutines[static_cast<std::size_t>(route)], fault);
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, plan, workspace, report); });
  const bool negative = fault == Fault::kNegativeError ||
                        fault == Fault::kMixedError ||
                        fault == Fault::kNegativeStats;
  ASC_DENSE_TEST_EQ(
      test, status.code(),
      negative ? asc::ErrorCode::kProvider : asc::ErrorCode::kNumerical);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  ASC_DENSE_TEST_CHECK(test, faults::SawInfoSentinel());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info,
                    fault == Fault::kAccuracy ? 4 : 0);
  ASC_DENSE_TEST_EQ(test, report.outcome,
                    negative ? asc::LapackOutcome::kPartialResult
                             : asc::LapackOutcome::kAccuracyWarning);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kDocumentedPartial);
  if (fault == Fault::kMixedError) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 1);
  } else if (fault == Fault::kNegativeError ||
             fault == Fault::kNonfiniteError) {
    ASC_DENSE_TEST_EQ(test, report.diagnostic_index, 0);
  } else {
    ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  }
  ASC_DENSE_TEST_CHECK(test, !report.factor_family.has_value());
  sample.data.x.Check(test, sample.data.original, before.data.x.values);
  sample.data.ferr.Check(test);
  sample.data.berr.Check(test);
  sample.data.pivots.Check(test);
  sample.data.rows.Check(test);
  sample.data.columns.Check(test);
  sample.data.af.Padding(test, before.data.af.values);
  scratch.Check(test);
  if (route == 2) {
    ASC_DENSE_TEST_CHECK(
        test, support::SameBytes(sample.data.a.values, before.data.a.values));
    ASC_DENSE_TEST_CHECK(
        test, support::SameBytes(sample.data.af.values, before.data.af.values));
    ASC_DENSE_TEST_CHECK(
        test, support::SameBytes(sample.data.b.values, before.data.b.values));
    ASC_DENSE_TEST_EQ(test, sample.data.pivots.values,
                      before.data.pivots.values);
  } else {
    driver::Scaling(test, sample.data, before.data, route - 4);
  }
}
template <typename T>
void AddedAcceptance(TestContext& test,
                     const asc::ReferenceLapackProvider& provider) {
  for (int route = 0; route < 7; ++route) {
    WorkspaceFailures<T>(test, provider, route);
  }
  InputFailures<T>(test, provider);
  for (const int route : {2, 4, 5, 6}) {
    for (const auto fault :
         {Fault::kNegativeError, Fault::kNonfiniteError, Fault::kMixedError}) {
      Quality<T>(test, provider, route, fault);
    }
    if (route != 2) {
      for (const auto fault :
           {Fault::kNegativeStats, Fault::kNonfiniteStats, Fault::kAccuracy}) {
        Quality<T>(test, provider, route, fault);
      }
    }
  }
}
template <typename T>
void Cold(TestContext& test, const asc::ReferenceLapackProvider& provider,
          int route) {
  Case<T> sample(test, provider, route, true);
  const auto before = sample;
  const auto plan = Take(
      support::WithoutAllocation(test, [&] { return sample.Query(provider); }));
  support::Scratch<T> scratch(plan);
  const auto workspace = scratch.View();
  asc::LapackReport report;
  faults::Select(kRoutines[static_cast<std::size_t>(route)], Fault::kPass);
  const auto status = support::WithoutAllocation(
      test, [&] { return sample.Execute(provider, plan, workspace, report); });
  ASC_DENSE_TEST_CHECK(
      test, status.ok() && report.called_provider && report.native_info == 0);
  ASC_DENSE_TEST_EQ(test, faults::Calls(), 1);
  ASC_DENSE_TEST_CHECK(test, faults::SawInfoSentinel());
  if (route == 0) {
    ASC_DENSE_TEST_EQ(test, sample.equilibration.row_condition, 1);
    ASC_DENSE_TEST_EQ(test, sample.equilibration.column_condition, 1);
  } else if (route == 1) {
    ASC_DENSE_TEST_EQ(test, sample.rcond, 1);
  } else if (route == 3) {
    sample.data.b.Check(test, sample.data.original, before.data.b.values);
    sample.data.af.Reconstruct(test, sample.data.pivots);
  } else {
    sample.data.x.Check(test, sample.data.original, before.data.x.values);
  }
  sample.data.rows.Check(test);
  sample.data.columns.Check(test);
  sample.data.pivots.Check(test);
  sample.data.ferr.Check(test);
  sample.data.berr.Check(test);
  scratch.Check(test);
  std::printf(
      "Fresh process route=%d: first foreign call and query allocate no "
      "storage\n",
      route);
}
template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (int route = 0; route < 7; ++route) {
    for (const auto fault :
         {Fault::kNegative, Fault::kMinimum, Fault::kLargePositive,
          Fault::kNoInfo, Fault::kPartialInfo}) {
      for (const bool prior : {false, true}) {
        NativeFailure<T>(test, provider, route, fault, prior);
      }
    }
    Structural<T>(test, provider, route);
  }
  for (const int route : {3, 4, 5}) {
    for (const auto fault :
         {Fault::kNoPivots, Fault::kPartialPivot, Fault::kLatePivot}) {
      for (const bool prior : {false, true}) {
        NativeFailure<T>(test, provider, route, fault, prior);
      }
    }
  }
  for (const int route : {4, 5, 6}) {
    NativeFailure<T>(test, provider, route, Fault::kInvalidEqued, false);
  }
  AddedAcceptance<T>(test, provider);
  std::printf(
      "91 matching-plan native failures plus seven stale/metadata-alias pairs; "
      "initially-zero/prior-valid native bytes, full-width INFO/pivot "
      "sentinels\n");
}
}  // namespace
int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2 && argc != 3) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar(argv[1]);
  const std::string_view mode = argc == 3 ? argv[2] : "all";
  if (mode != "all" && (mode.size() != 1 || mode[0] < '0' || mode[0] > '6')) {
    return 2;
  }
  const auto run = [&]<typename T>() {
    if (mode == "all") {
      Run<T>(test, provider);
    } else {
      Cold<T>(test, provider, mode[0] - '0');
    }
  };
  if (scalar == "s") {
    run.template operator()<float>();
  } else if (scalar == "d") {
    run.template operator()<double>();
  } else if (scalar == "c") {
    run.template operator()<std::complex<float>>();
  } else if (scalar == "z") {
    run.template operator()<std::complex<double>>();
  } else {
    return 2;
  }
  return test.Finish();
}
