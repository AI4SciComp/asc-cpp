#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_tridiagonal.h"
#include "asc/dense/providers/lapack_tridiagonal_condition.h"
#include "asc/dense/providers/lapack_tridiagonal_driver.h"
#include "asc/dense/providers/lapack_tridiagonal_refinement.h"
#include "installed_lu/normal_return_guard.h"
#include "tridiagonal_faults.h"
#include "tridiagonal_test_support.h"

namespace {
using asc_tridiagonal_fault::Mode;
using asc_tridiagonal_fault::Operation;
using asc_tridiagonal_test::EqualBytes;
using asc_tridiagonal_test::Exact;
using asc_tridiagonal_test::kCapacity;
using asc_tridiagonal_test::kColumn;
using asc_tridiagonal_test::kHost;
using asc_tridiagonal_test::kInteger;
using asc_tridiagonal_test::kNone;
using asc_tridiagonal_test::kPivot;
using asc_tridiagonal_test::kRow;
using asc_tridiagonal_test::Narrow;
using asc_tridiagonal_test::Pivots;
using asc_tridiagonal_test::Rhs;
using asc_tridiagonal_test::RightHandSides;
using asc_tridiagonal_test::Scratch;
using asc_tridiagonal_test::Take;
using asc_tridiagonal_test::TestContext;
using asc_tridiagonal_test::Tri;
using asc_tridiagonal_test::Vector;
using asc_tridiagonal_test::WithoutAllocation;
namespace fault = asc_tridiagonal_fault;

template <typename T>
struct Case {
  using Real = asc::DenseBlasRealType<T>;
  Tri<T> original;
  Tri<T> factors;
  std::array<asc::index_t, kCapacity + 2> pivots{};
  Rhs<T> rhs;
  Rhs<T> solution;
  std::array<Real, 8> ferr{};
  std::array<Real, 8> berr{};
  Real rcond = -95;
  std::optional<asc::ReferenceTridiagonalLuFactorView<T>> factor;
  asc::LapackReport factor_report;
  explicit Case(const asc::ReferenceLapackProvider& provider, int n = 3,
                int nrhs = 2)
      : original(n), factors(n), rhs(n, nrhs, kRow), solution(n, nrhs, kRow) {
    original.Initialize(0, 1);
    factors = original;
    pivots.fill(-91);
    ferr.fill(Real{-97});
    berr.fill(Real{-99});
    RightHandSides(original, kNone, rhs);
    for (int j = 0; j < nrhs; ++j) {
      for (int i = 0; i < n; ++i) {
        solution.At(i, j) = Narrow<T>(Exact<T>(i, j));
      }
    }
    fault::Reset(Operation::kFactor);
    const auto plan = Take(asc::QueryGttrfWorkspace(provider, factors.Factors(),
                                                    Vector(pivots, n)));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    if (!asc::Gttrf(provider, factors.Factors(), Vector(pivots, n), plan,
                    workspace, factor_report)
             .ok()) {
      std::abort();
    }
    factor.emplace(Take(asc::ReferenceTridiagonalLuFactorView<T>::Create(
        provider, std::as_const(factors).Factors(), Pivots(pivots, n),
        factor_report)));
  }
  auto Query(const asc::ReferenceLapackProvider& provider, Operation operation,
             bool factored = false) {
    const auto n = original.order;
    if (!factor.has_value()) {
      std::abort();
    }
    switch (operation) {
      case Operation::kFactor:
        return asc::QueryGttrfWorkspace(provider, factors.Factors(),
                                        Vector(pivots, n));
      case Operation::kSolve:
        return asc::QueryGttrsWorkspace(provider, kNone, *factor, rhs.View());
      case Operation::kDriver:
        return asc::QueryGtsvWorkspace(provider, factors.View(), rhs.View());
      case Operation::kCondition:
        return asc::QueryGtconWorkspace(provider,
                                        asc::LapackConditionNorm::kOne,
                                        std::as_const(factors).Factors(),
                                        Pivots(pivots, n), Real{6}, rcond);
      case Operation::kRefinement:
        return asc::QueryGtrfsWorkspace(
            provider, kNone, std::as_const(original).View(),
            std::as_const(factors).Factors(), Pivots(pivots, n),
            std::as_const(rhs).View(), solution.View(),
            Vector(ferr, rhs.columns), Vector(berr, rhs.columns));
      case Operation::kExpert:
        if (factored) {
          return asc::QueryGtsvxFactoredWorkspace(
              provider, kNone, std::as_const(original).View(),
              std::as_const(factors).Factors(), Pivots(pivots, n),
              std::as_const(rhs).View(), solution.View(), rcond,
              Vector(ferr, rhs.columns), Vector(berr, rhs.columns));
        }
        return asc::QueryGtsvxWorkspace(
            provider, kNone, std::as_const(original).View(), factors.Factors(),
            Vector(pivots, n), std::as_const(rhs).View(), solution.View(),
            rcond, Vector(ferr, rhs.columns), Vector(berr, rhs.columns));
    }
    std::abort();
  }
  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      Operation operation, const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report, bool factored = false) {
    const auto n = original.order;
    if (!factor.has_value()) {
      std::abort();
    }
    switch (operation) {
      case Operation::kFactor:
        return asc::Gttrf(provider, factors.Factors(), Vector(pivots, n), plan,
                          workspace, report);
      case Operation::kSolve:
        return asc::Gttrs(provider, kNone, *factor, rhs.View(), plan, workspace,
                          report);
      case Operation::kDriver:
        return asc::Gtsv(provider, factors.View(), rhs.View(), plan, workspace,
                         report);
      case Operation::kCondition:
        return asc::Gtcon(provider, asc::LapackConditionNorm::kOne,
                          std::as_const(factors).Factors(), Pivots(pivots, n),
                          Real{6}, rcond, plan, workspace, report);
      case Operation::kRefinement:
        return asc::Gtrfs(provider, kNone, std::as_const(original).View(),
                          std::as_const(factors).Factors(), Pivots(pivots, n),
                          std::as_const(rhs).View(), solution.View(),
                          Vector(ferr, rhs.columns), Vector(berr, rhs.columns),
                          plan, workspace, report);
      case Operation::kExpert:
        if (factored) {
          return asc::GtsvxFactored(
              provider, kNone, std::as_const(original).View(),
              std::as_const(factors).Factors(), Pivots(pivots, n),
              std::as_const(rhs).View(), solution.View(), rcond,
              Vector(ferr, rhs.columns), Vector(berr, rhs.columns), plan,
              workspace, report);
        }
        return asc::Gtsvx(provider, kNone, std::as_const(original).View(),
                          factors.Factors(), Vector(pivots, n),
                          std::as_const(rhs).View(), solution.View(), rcond,
                          Vector(ferr, rhs.columns), Vector(berr, rhs.columns),
                          plan, workspace, report);
    }
    std::abort();
  }
  void Unchanged(TestContext& test, const Case& before) const {
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(&original, &before.original, sizeof(original)));
    ASC_DENSE_TEST_CHECK(
        test, EqualBytes(&factors, &before.factors, sizeof(factors)));
    ASC_DENSE_TEST_EQ(test, pivots, before.pivots);
    ASC_DENSE_TEST_EQ(test, rhs.data, before.rhs.data);
    ASC_DENSE_TEST_EQ(test, solution.data, before.solution.data);
    ASC_DENSE_TEST_EQ(test, ferr, before.ferr);
    ASC_DENSE_TEST_EQ(test, berr, before.berr);
    ASC_DENSE_TEST_EQ(test, rcond, before.rcond);
  }
};

template <typename T>
std::size_t WorkspaceDefects(TestContext& test,
                             const asc::ReferenceLapackProvider& provider,
                             Case<T>& data, Operation operation, bool factored,
                             const asc::LapackWorkspacePlan& plan) {
  std::size_t count = 0;
  for (std::size_t region = 0; region < plan.regions.size(); ++region) {
    if (plan.regions[region].minimum_entries == 0) {
      continue;
    }
    for (const int defect : {0, 1, 2, 3}) {
      Scratch<T> scratch;
      auto workspace = scratch.Workspace(plan);
      auto current = workspace.regions[region];
      if (defect == 0) {
        workspace.regions[region] = {current.data(), current.size() - 1, kHost};
      } else if (defect == 1) {
        workspace.regions[region] = {
            static_cast<std::byte*>(current.data()) + 1, current.size(), kHost};
      } else if (defect == 2) {
        workspace.regions[region] = {current.data(), current.size(),
                                     asc::MemorySpace::kPinnedHost};
      } else {
        workspace.regions[region] = {current.data(), current.size(),
                                     asc::MemorySpace::kDevice};
      }
      const auto before = data;
      const auto scratch_before = scratch;
      asc::LapackReport report;
      report.native_info = 123;
      report.called_provider = true;
      fault::Reset(operation);
      const auto status = WithoutAllocation(test, [&] {
        return data.Execute(provider, operation, plan, workspace, report,
                            factored);
      });
      ASC_DENSE_TEST_CHECK(test, !status.ok());
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
      ASC_DENSE_TEST_EQ(test, report.called_provider, false);
      ASC_DENSE_TEST_EQ(test, report.native_info.has_value(), false);
      data.Unchanged(test, before);
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(&scratch, &scratch_before, sizeof(scratch)));
      ++count;
    }
  }
  return count;
}

template <typename T>
std::size_t PlanDefects(TestContext& test,
                        const asc::ReferenceLapackProvider& provider,
                        Case<T>& data, Operation operation, bool factored,
                        const asc::LapackWorkspacePlan& plan) {
  std::size_t count = 0;
  Scratch<T> scratch;
  auto workspace = scratch.Workspace(plan);
  auto changed = plan;
  ++changed.total_byte_limit;  // wraps from max: still a stale plan.
  const auto before = data;
  asc::LapackReport report;
  fault::Reset(operation);
  const auto status =
      data.Execute(provider, operation, changed, workspace, report, factored);
  ASC_DENSE_TEST_CHECK(test, !status.ok());
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  data.Unchanged(test, before);
  if (operation != Operation::kFactor && operation != Operation::kDriver &&
      (operation != Operation::kExpert || factored)) {
    data.pivots[1] = 0;
    fault::Reset(operation);
    const auto value_free = data.Query(provider, operation, factored);
    ASC_DENSE_TEST_CHECK(test, value_free.ok());
    const auto bad_pivot_before = data;
    const auto bad_status =
        data.Execute(provider, operation, plan, workspace, report, factored);
    ASC_DENSE_TEST_EQ(test, bad_status.code(), asc::ErrorCode::kIndex);
    ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
    data.Unchanged(test, bad_pivot_before);
    ++count;
  }
  return count;
}

template <typename T>
void Preflight(TestContext& test,
               const asc::ReferenceLapackProvider& provider) {
  std::size_t count = 0;
  for (const auto operation :
       {Operation::kFactor, Operation::kSolve, Operation::kDriver,
        Operation::kCondition, Operation::kRefinement, Operation::kExpert}) {
    for (const bool factored : {false, true}) {
      Case<T> data(provider);
      fault::Reset(operation);
      const auto plan = Take(WithoutAllocation(
          test, [&] { return data.Query(provider, operation, factored); }));
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
      count +=
          WorkspaceDefects(test, provider, data, operation, factored, plan);
      count += PlanDefects(test, provider, data, operation, factored, plan);
    }
  }
  // Real valid 1x1 matrices admit unused extreme original strides in both
  // layouts. The private effective LD is one; no forged backing span exists.
  for (const auto layout : {kColumn, kRow}) {
    Case<T> data(provider, 1, 1);
    data.rhs.layout = layout;
    data.rhs.ld = std::numeric_limits<asc::extent_t>::max();
    data.solution.layout = layout;
    data.solution.ld = std::numeric_limits<asc::extent_t>::max();
    for (const auto operation : {Operation::kSolve, Operation::kDriver,
                                 Operation::kRefinement, Operation::kExpert}) {
      fault::Reset(operation);
      const auto plan = Take(data.Query(provider, operation));
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      ASC_DENSE_TEST_CHECK(
          test,
          data.Execute(provider, operation, plan, workspace, report).ok());
      ASC_DENSE_TEST_CHECK(test, fault::FocusCalls() > 0);
    }
  }
  // Live report.native_info is a real index_t object, not invented backing.
  Case<T> data(provider, 1, 1);
  asc::LapackReport report;
  report.native_info = 73;
  auto* live = &*report.native_info;
  const auto alias = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      live, 1, 1, {live, sizeof(*live), kHost}));
  const auto plan =
      Take(asc::QueryGttrfWorkspace(provider, data.factors.Factors(), alias));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  const auto report_before = report;
  fault::Reset(Operation::kFactor);
  const auto status = asc::Gttrf(provider, data.factors.Factors(), alias, plan,
                                 workspace, report);
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, report.native_info, report_before.native_info);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  std::printf(
      "%zu short/misaligned/context/stale/borrowed-pivot checks; "
      "wide-valid strides and live report alias\n",
      count);
}

template <typename T>
void NativeSegments(TestContext& test,
                    const asc::ReferenceLapackProvider& provider) {
  for (const int n : {1, 2, 3, 4, 7}) {
    for (const auto operation :
         {Operation::kCondition, Operation::kRefinement, Operation::kExpert}) {
      Case<T> data(provider, n, 1);
      const auto plan = Take(data.Query(provider, operation));
      const auto integer_count = asc::DenseBlasReal<T> ? 2 * n : n;
      ASC_DENSE_TEST_EQ(test, plan.regions[kInteger].minimum_entries,
                        integer_count);
      ASC_DENSE_TEST_EQ(test, plan.regions[kPivot].minimum_entries, 0);
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      asc::LapackReport report;
      fault::Reset(operation);
      ASC_DENSE_TEST_CHECK(
          test,
          data.Execute(provider, operation, plan, workspace, report).ok());
      ASC_DENSE_TEST_CHECK(test, fault::IntegerSegmentsValid());
      ASC_DENSE_TEST_CHECK(test, fault::CharacterLengthsValid());
      scratch.Guards(test, workspace);
    }
    Case<T> data(provider, n, 0);
    const auto before = data.rhs.data;
    const auto plan = Take(data.Query(provider, Operation::kDriver));
    Scratch<T> scratch;
    const auto workspace = scratch.Workspace(plan);
    asc::LapackReport report;
    fault::Reset(Operation::kDriver);
    ASC_DENSE_TEST_CHECK(test, data.Execute(provider, Operation::kDriver, plan,
                                            workspace, report)
                                   .ok());
    ASC_DENSE_TEST_EQ(test, fault::FocusCalls(), 1U);
    ASC_DENSE_TEST_EQ(test, fault::ZeroRhsObserved(), asc::DenseBlasReal<T>);
    ASC_DENSE_TEST_EQ(test, data.rhs.data, before);
    scratch.Guards(test, workspace);
  }
}

template <typename T>
void Faults(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto operation :
       {Operation::kFactor, Operation::kSolve, Operation::kDriver,
        Operation::kCondition, Operation::kRefinement, Operation::kExpert}) {
    for (const auto mode :
         {Mode::kNegativeInfo, Mode::kExcessInfo, Mode::kInvalidPivot,
          Mode::kNegativeDiagnostic, Mode::kNonfiniteDiagnostic,
          Mode::kWarningNegativeError}) {
      if (mode == Mode::kInvalidPivot && operation != Operation::kFactor &&
          operation != Operation::kExpert) {
        continue;
      }
      if ((mode == Mode::kNegativeDiagnostic ||
           mode == Mode::kNonfiniteDiagnostic) &&
          operation != Operation::kCondition &&
          operation != Operation::kRefinement &&
          operation != Operation::kExpert) {
        continue;
      }
      if (mode == Mode::kWarningNegativeError &&
          operation != Operation::kExpert) {
        continue;
      }
      Case<T> data(provider);
      const auto plan = Take(data.Query(provider, operation));
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      const auto pivots_before = data.pivots;
      asc::LapackReport report;
      fault::Reset(operation, mode);
      const auto status = WithoutAllocation(test, [&] {
        return data.Execute(provider, operation, plan, workspace, report);
      });
      ASC_DENSE_TEST_EQ(test, report.called_provider, true);
      ASC_DENSE_TEST_EQ(test, fault::FocusCalls(), 1U);
      ASC_DENSE_TEST_CHECK(test, fault::CharacterLengthsValid());
      ASC_DENSE_TEST_CHECK(test, fault::IntegerSegmentsValid());
      ASC_DENSE_TEST_EQ(test, status.code(),
                        mode == Mode::kNonfiniteDiagnostic
                            ? asc::ErrorCode::kNumerical
                            : asc::ErrorCode::kProvider);
      if (mode == Mode::kNegativeInfo) {
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), -1);
        ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(0), 1);
      } else if (mode == Mode::kWarningNegativeError) {
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 4);
      }
      if (mode == Mode::kInvalidPivot) {
        ASC_DENSE_TEST_EQ(test, data.pivots, pivots_before);
      }
      scratch.Guards(test, workspace);
    }
  }
  NativeSegments<T>(test, provider);
}

template <typename T>
void PivotOutputDefects(TestContext& test,
                        const asc::ReferenceLapackProvider& provider) {
  for (const auto operation : {Operation::kFactor, Operation::kExpert}) {
    for (const auto mode : {Mode::kUnwrittenPivot, Mode::kPartialPivotWidth}) {
      for (const bool prior_valid : {false, true}) {
        Case<T> data(provider, 1, 1);
        const auto plan = Take(data.Query(provider, operation));
        const auto width = plan.regions[kInteger].entry_bytes;
        if (mode == Mode::kPartialPivotWidth && width != sizeof(std::int64_t)) {
          continue;  // This injection specifically writes half of actual ILP64.
        }
        Scratch<T> scratch;
        const auto workspace = scratch.Workspace(plan);
        auto region = workspace.regions[kInteger];
        std::memset(region.data(), 0, region.size());
        if (prior_valid) {
          if (width == sizeof(std::int64_t)) {
            const std::int64_t value = 1;
            std::memcpy(region.data(), &value, sizeof(value));
          } else {
            const std::int32_t value = 1;
            std::memcpy(region.data(), &value, sizeof(value));
          }
        }
        data.rcond = 1;
        data.ferr[1] = 0;
        data.berr[1] = 0;
        const auto pivots_before = data.pivots;
        const auto solution_before = data.solution.data;
        fault::Reset(operation, mode);
        asc::LapackReport report;
        const auto status = WithoutAllocation(test, [&] {
          return data.Execute(provider, operation, plan, workspace, report);
        });
        std::printf(
            "pivot output fault: operation=%d mode=%d prior_valid=%d "
            "native_bytes=%zu status=%d info=%lld\n",
            static_cast<int>(operation), static_cast<int>(mode),
            static_cast<int>(prior_valid), width,
            static_cast<int>(status.code()),
            static_cast<long long>(report.native_info.value_or(-999)));
        ASC_DENSE_TEST_EQ(test, fault::FocusCalls(), 1U);
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
        ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnusable);
        ASC_DENSE_TEST_EQ(test, data.pivots, pivots_before);
        ASC_DENSE_TEST_EQ(test, data.solution.data, solution_before);
        scratch.Guards(test, workspace);
      }
    }
  }
}

template <typename T>
void InfoOutputDefects(TestContext& test,
                       const asc::ReferenceLapackProvider& provider) {
  for (const auto operation :
       {Operation::kFactor, Operation::kSolve, Operation::kDriver,
        Operation::kCondition, Operation::kRefinement, Operation::kExpert}) {
    for (const bool factored : {false, true}) {
      if (factored && operation != Operation::kExpert) {
        continue;
      }
      Case<T> data(provider, 1, 1);
      const auto plan = Take(data.Query(provider, operation, factored));
      const auto factor_plan = Take(data.Query(provider, Operation::kFactor));
      const auto width = factor_plan.regions[kInteger].entry_bytes;
      const std::int64_t sentinel =
          width == sizeof(std::int64_t)
              ? std::numeric_limits<std::int64_t>::min()
              : std::numeric_limits<std::int32_t>::min();
      Scratch<T> scratch;
      const auto workspace = scratch.Workspace(plan);
      const auto pivots_before = data.pivots;
      fault::Reset(operation, Mode::kUnwrittenInfo);
      asc::LapackReport report;
      const auto status = WithoutAllocation(test, [&] {
        return data.Execute(provider, operation, plan, workspace, report,
                            factored);
      });
      std::printf(
          "INFO withheld after real call: operation=%d factored=%d "
          "status=%d info=%lld\n",
          static_cast<int>(operation), static_cast<int>(factored),
          static_cast<int>(status.code()),
          static_cast<long long>(report.native_info.value_or(-999)));
      ASC_DENSE_TEST_EQ(test, fault::FocusCalls(), 1U);
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
      ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), sentinel);
      ASC_DENSE_TEST_CHECK(test, report.called_provider);
      ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
      ASC_DENSE_TEST_EQ(test, report.output_validity,
                        asc::LapackOutputValidity::kUnusable);
      ASC_DENSE_TEST_EQ(test, data.pivots, pivots_before);
      ASC_DENSE_TEST_CHECK(test, fault::IntegerSegmentsValid());
      ASC_DENSE_TEST_CHECK(test, fault::CharacterLengthsValid());
      scratch.Guards(test, workspace);
    }
  }
}

template <typename T>
void MetadataOutputDefects(TestContext& test,
                           const asc::ReferenceLapackProvider& provider) {
  for (const auto operation :
       {Operation::kFactor, Operation::kSolve, Operation::kDriver,
        Operation::kCondition, Operation::kRefinement, Operation::kExpert}) {
    for (const bool factored : {false, true}) {
      if (factored && operation != Operation::kExpert) {
        continue;
      }
      Case<T> data(provider, 1, 1);
      auto plan = Take(data.Query(provider, operation, factored));
      Scratch<T> scratch;
      auto workspace = scratch.Workspace(plan);
      for (std::size_t i = 0; i < plan.regions.size(); ++i) {
        if (plan.regions[i].minimum_entries == 0) {
          continue;
        }
        const auto bytes = workspace.regions[i].size();
        ASC_DENSE_TEST_CHECK(test, bytes <= sizeof(plan));
        ASC_DENSE_TEST_EQ(
            test,
            reinterpret_cast<std::uintptr_t>(&plan) % plan.regions[i].alignment,
            0U);
        // Exact capacity, aligned, real metadata backing; no unrelated short
        // workspace or stale plan can mask metadata-alias report preservation.
        workspace.regions[i] = {&plan, bytes, kHost};
        break;
      }
      const auto before = data;
      const auto scratch_before = scratch;
      asc::LapackReport report = data.factor_report;
      std::array<std::byte, sizeof(report)> report_before{};
      std::memcpy(report_before.data(), &report, sizeof(report));
      fault::Reset(operation);
      const auto status = WithoutAllocation(test, [&] {
        return data.Execute(provider, operation, plan, workspace, report,
                            factored);
      });
      ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
      ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(&report, report_before.data(), sizeof(report)));
      data.Unchanged(test, before);
      ASC_DENSE_TEST_CHECK(
          test, EqualBytes(&scratch, &scratch_before, sizeof(scratch)));
    }
  }
  Case<T> data(provider, 1, 1);
  auto plan = Take(data.Query(provider, Operation::kFactor));
  auto* live = &plan.regions[kInteger].minimum_entries;
  const auto alias = Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      live, 1, 1, {live, sizeof(*live), kHost}));
  Scratch<T> scratch;
  const auto workspace = scratch.Workspace(plan);
  asc::LapackReport report = data.factor_report;
  std::array<std::byte, sizeof(report)> report_before{};
  std::memcpy(report_before.data(), &report, sizeof(report));
  const auto before = data;
  const auto scratch_before = scratch;
  fault::Reset(Operation::kFactor);
  const auto status = WithoutAllocation(test, [&] {
    return asc::Gttrf(provider, data.factors.Factors(), alias, plan, workspace,
                      report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, fault::Calls(), 0U);
  ASC_DENSE_TEST_CHECK(
      test, EqualBytes(&report, report_before.data(), sizeof(report)));
  data.Unchanged(test, before);
  ASC_DENSE_TEST_CHECK(test,
                       EqualBytes(&scratch, &scratch_before, sizeof(scratch)));
}

}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2 && (argc != 3 || std::string_view(argv[2]) != "boundary")) {
    return 2;
  }
  TestContext test;
  const auto provider = Take(
      asc::ReferenceLapackProvider::Create(asc::ExecutionContext::Serial()));
  const std::string_view scalar = argv[1];
  const auto run = [&]<typename T>() {
    if (argc == 3) {
      PivotOutputDefects<T>(test, provider);
      InfoOutputDefects<T>(test, provider);
      MetadataOutputDefects<T>(test, provider);
    } else {
      Preflight<T>(test, provider);
      Faults<T>(test, provider);
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
