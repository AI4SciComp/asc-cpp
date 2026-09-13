#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

#include "../../src/dense/lapack/internal_band_abi.h"
#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_condition.h"
#include "band_cholesky_test_support.h"
#include "band_condition_fault_support.h"
#include "band_estimation_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_condition_test::FaultArgumentsValid;
using asc_band_condition_test::FaultCalls;
using asc_band_condition_test::ResetFault;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> factors;
  std::array<Real, 3> result{Real{-241}, Real{-251}, Real{-257}};
  Fixture(asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout)
      : factors(3, 1, triangle, layout) {}
  auto Query(const asc::ReferenceLapackProvider& provider,
             asc::MemorySpace space = kHost, Real norm = Real{1}) {
    return asc::QueryPbconWorkspace(provider, factors.ConstView(space), norm,
                                    result[1]);
  }
  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report, asc::MemorySpace space = kHost,
                      Real norm = Real{1}) {
    return asc::Pbcon(provider, factors.ConstView(space), norm, result[1], plan,
                      workspace, report);
  }
};

template <typename T>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace, asc::ErrorCode code,
              asc::MemorySpace space = kHost,
              asc::DenseBlasRealType<T> norm = 1) {
  const auto before = data.factors.values;
  const auto result = data.result;
  std::array<std::vector<std::byte>, 8> bytes;
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    const auto region = workspace.regions[i];
    if (region.data() != nullptr && region.size() != 0) {
      const auto* first = static_cast<const std::byte*>(region.data());
      bytes[i].assign(first, first + region.size());
    }
  }
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 73;
  ResetFault(0, 0.5);
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, plan, workspace, report, space, norm);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), code);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.factors.values));
  ASC_DENSE_TEST_EQ(test, result, data.result);
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (!bytes[i].empty()) {
      ASC_DENSE_TEST_CHECK(test, std::equal(bytes[i].begin(), bytes[i].end(),
                                            static_cast<const std::byte*>(
                                                workspace.regions[i].data())));
    }
  }
}

template <typename T>
void Placement(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace) {
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    if (space == asc::MemorySpace::kPinnedHost) {
      ASC_DENSE_TEST_EQ(
          test,
          WithoutAllocation(test, [&] { return data.Query(provider, space); })
              .status()
              .code(),
          asc::ErrorCode::kMemoryAccess);
      Rejected(test, provider, data, plan, workspace,
               asc::ErrorCode::kMemoryAccess, space);
    }
    for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
      alignas(64) std::array<T, 128> scratch{};
      auto bad = workspace;
      bad.regions[role] = {scratch.data(), sizeof(scratch), space};
      Rejected(test, provider, data, plan, bad, asc::ErrorCode::kMemoryAccess);
    }
  }
  auto good = workspace;
  constexpr auto kUnused =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
  good.regions[kUnused] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
  ResetFault(0, 0.5);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return data.Execute(provider, plan, good,
                                                   report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
}

template <typename T>
void Freshness(TestContext& test, const asc::ReferenceLapackProvider& provider,
               Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace) {
  for (std::size_t role = 0; role < plan.regions.size(); ++role) {
    for (const int field : {0, 1, 2, 3}) {
      auto stale = plan;
      auto& region = stale.regions[role];
      if (field == 0) {
        ++region.minimum_entries;
      } else if (field == 1) {
        ++region.preferred_entries;
      } else if (field == 2) {
        ++region.entry_bytes;
      } else {
        region.alignment *= 2;
      }
      Rejected(test, provider, data, stale, workspace,
               asc::ErrorCode::kInvalidState);
    }
  }
  auto stale = plan;
  --stale.total_byte_limit;
  Rejected(test, provider, data, stale, workspace,
           asc::ErrorCode::kInvalidState);
  stale = plan;
  stale.identity = Take(asc::LapackPlanIdentity::Create(
      "not_pbcon", plan.identity.scalar(), {}, {}, provider.identity()));
  Rejected(test, provider, data, stale, workspace,
           asc::ErrorCode::kInvalidState);
  Rejected(test, provider, data, plan, workspace, asc::ErrorCode::kInvalidState,
           kHost, 0);
}

template <typename T>
void WorkspaceFailures(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
                       const asc::LapackWorkspace& workspace) {
  constexpr auto kUnused =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
  auto bad = workspace;
  bad.regions[kUnused] = {nullptr, sizeof(T), kHost};
  Rejected(test, provider, data, plan, bad, asc::ErrorCode::kMemoryAccess);
  bad.regions[kUnused] = {data.factors.values.data() + 1, sizeof(T), kHost};
  Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
  bad.regions[kUnused] = {&data.result[1], sizeof(data.result[1]), kHost};
  Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
  for (std::size_t role = 0; role < plan.regions.size(); ++role) {
    if (plan.regions[role].minimum_entries == 0) {
      continue;
    }
    bad = workspace;
    const auto region = workspace.regions[role];
    bad.regions[role] = {region.data(), region.size() - 1, kHost};
    Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
    bad.regions[role] = {static_cast<std::byte*>(region.data()) + 1,
                         region.size(), kHost};
    Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
    bad = workspace;
    bad.regions[kUnused] = region;
    Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void MetadataAlias(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace) {
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 73;
  auto bad = workspace;
  constexpr auto kUnused =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
  bad.regions[kUnused] = {&report, sizeof(report), kHost};
  ResetFault(0, 0.5);
  const auto before = data.factors.values;
  const auto result = data.result;
  ASC_DENSE_TEST_EQ(
      test,
      WithoutAllocation(
          test, [&] { return data.Execute(provider, plan, bad, report); })
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 73);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.factors.values));
  ASC_DENSE_TEST_EQ(test, result, data.result);
}

template <typename T>
void InvalidNorm(TestContext& test,
                 const asc::ReferenceLapackProvider& provider, Fixture<T>& data,
                 const asc::LapackWorkspacePlan& plan,
                 const asc::LapackWorkspace& workspace) {
  using Real = asc::DenseBlasRealType<T>;
  for (const Real norm : {Real{-1}, std::numeric_limits<Real>::infinity(),
                          std::numeric_limits<Real>::quiet_NaN()}) {
    ASC_DENSE_TEST_EQ(
        test,
        WithoutAllocation(test,
                          [&] { return data.Query(provider, kHost, norm); })
            .status()
            .code(),
        asc::ErrorCode::kInvalidArgument);
    Rejected(test, provider, data, plan, workspace,
             asc::ErrorCode::kInvalidArgument, kHost, norm);
  }
}

void CheckInfo(TestContext& test, const asc::LapackReport& report,
               const asc::Status& status, std::int64_t info, bool quality) {
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(73), info);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.diagnostic_index.has_value(), false);
  if (info < 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kProviderArgument);
    if (info == std::numeric_limits<std::int64_t>::min()) {
      ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
    } else {
      ASC_DENSE_TEST_EQ(test, report.native_argument.value_or(0), -info);
    }
  } else if (info > 0) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kPartialResult);
  } else {
    ASC_DENSE_TEST_EQ(test, status.ok(), !quality);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      quality ? asc::LapackOutcome::kAccuracyWarning
                              : asc::LapackOutcome::kSuccess);
  }
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    info != 0 ? asc::LapackOutputValidity::kUnusable
                    : quality ? asc::LapackOutputValidity::kDocumentedPartial
                              : asc::LapackOutputValidity::kComplete);
}

template <typename T>
void Fault(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
           std::int64_t info, double raw) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> data(triangle, layout);
  const auto plan = Take(data.Query(provider));
  WorkspaceStorage<T> storage(plan);
  const auto before = data.factors.values;
  asc::LapackReport report;
  ResetFault(info, raw);
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, plan, storage.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
  CheckInfo(test, report, status, info, !std::isfinite(raw) || raw < 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_CHECK(test,
                       SameScalarBytes(data.result[1], static_cast<Real>(raw)));
  ASC_DENSE_TEST_EQ(test, data.result.front(), Real{-241});
  ASC_DENSE_TEST_EQ(test, data.result.back(), Real{-257});
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.factors.values));
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      Fixture<T> data(triangle, layout);
      const auto plan = Take(data.Query(provider));
      WorkspaceStorage<T> storage(plan);
      Placement(test, provider, data, plan, storage.View());
      Freshness(test, provider, data, plan, storage.View());
      WorkspaceFailures(test, provider, data, plan, storage.View());
      MetadataAlias(test, provider, data, plan, storage.View());
      InvalidNorm(test, provider, data, plan, storage.View());
      for (const std::int64_t info :
           {std::int64_t{0}, std::int64_t{-6}, std::int64_t{1},
            static_cast<std::int64_t>(
                std::numeric_limits<lapack_int>::min())}) {
        Fault<T>(test, provider, triangle, layout, info, 0.5);
      }
      for (const double raw :
           {0.0, -1.0, std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::quiet_NaN()}) {
        Fault<T>(test, provider, triangle, layout, 0, raw);
      }
      storage.Check(test);
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
  const asc_lapack_test::NormalReturnGuard return_guard;
  if (argc != 2) {
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
