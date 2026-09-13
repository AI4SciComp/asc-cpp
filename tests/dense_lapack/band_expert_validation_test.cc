#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <string_view>
#include <vector>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_expert_driver_fault_support.h"
#include "band_expert_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_driver_test::FaultArgumentsValid;
using asc_band_driver_test::FaultCalls;
using asc_band_driver_test::Fixture;
using asc_band_driver_test::kHostSpaces;
using asc_band_driver_test::ResetFault;
using asc_band_estimation_test::WorkspaceStorage;

template <typename T>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace, asc::ErrorCode code,
              const std::array<asc::MemorySpace, 7>& spaces = kHostSpaces) {
  const auto a = data.data.a.values;
  const auto af = data.data.af.values;
  const auto b = data.data.b.values;
  const auto x = data.data.x.values;
  const auto ferr = data.data.ferr;
  const auto berr = data.data.berr;
  const auto scales = data.scales;
  const auto rcond = data.reciprocal_condition;
  const auto equed = data.equilibration;
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
  ResetFault();
  const auto status = WithoutAllocation(test, [&] {
    data.spaces = spaces;
    return data.Execute(provider, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), code);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  data.spaces = kHostSpaces;
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b, data.data.b.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(x, data.data.x.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(ferr, data.data.ferr));
  ASC_DENSE_TEST_CHECK(test, SameBytes(berr, data.data.berr));
  ASC_DENSE_TEST_CHECK(test, SameBytes(scales, data.scales));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(rcond, data.reciprocal_condition));
  ASC_DENSE_TEST_EQ(test, equed, data.equilibration);
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
    for (std::size_t operand = 0; operand < (data.mode == 'N' ? 6U : 7U);
         ++operand) {
      if (operand < 2 && space != asc::MemorySpace::kPinnedHost) {
        continue;
      }
      auto spaces = kHostSpaces;
      spaces[operand] = space;
      ASC_DENSE_TEST_EQ(test,
                        WithoutAllocation(test,
                                          [&] {
                                            data.spaces = spaces;
                                            return data.Query(provider);
                                          })
                            .status()
                            .code(),
                        asc::ErrorCode::kMemoryAccess);
      Rejected(test, provider, data, plan, workspace,
               asc::ErrorCode::kMemoryAccess, spaces);
    }
    for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
      alignas(64) std::array<T, 256> scratch{};
      auto bad = workspace;
      bad.regions[role] = {scratch.data(), sizeof(scratch), space};
      Rejected(test, provider, data, plan, bad, asc::ErrorCode::kMemoryAccess);
    }
  }
  data.spaces = kHostSpaces;
  constexpr auto kUnused =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
  auto good = workspace;
  good.regions[kUnused] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
  ResetFault();
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
      "not_pbsvx", plan.identity.scalar(), {}, {}, provider.identity()));
  Rejected(test, provider, data, stale, workspace,
           asc::ErrorCode::kInvalidState);
}

template <typename T>
void WorkFailures(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace) {
  constexpr auto kUnused =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
  auto bad = workspace;
  bad.regions[kUnused] = {nullptr, sizeof(T), kHost};
  Rejected(test, provider, data, plan, bad, asc::ErrorCode::kMemoryAccess);
  for (void* operand : {static_cast<void*>(data.data.a.values.data() + 1),
                        static_cast<void*>(data.data.af.values.data() + 1),
                        static_cast<void*>(data.data.b.values.data() + 1),
                        static_cast<void*>(data.data.x.values.data() + 1),
                        static_cast<void*>(data.data.ferr.data() + 1),
                        static_cast<void*>(data.data.berr.data() + 1),
                        static_cast<void*>(&data.reciprocal_condition)}) {
    bad = workspace;
    bad.regions[kUnused] = {operand, sizeof(asc::DenseBlasRealType<T>), kHost};
    Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
  }
  if (data.mode != 'N') {
    bad = workspace;
    bad.regions[kUnused] = {data.scales.data() + 1,
                            sizeof(asc::DenseBlasRealType<T>), kHost};
    Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
  }
  if (data.mode == 'E') {
    bad = workspace;
    bad.regions[kUnused] = {&data.equilibration, sizeof(data.equilibration),
                            kHost};
    Rejected(test, provider, data, plan, bad, asc::ErrorCode::kInvalidArgument);
  }
  for (std::size_t role = 0; role < plan.regions.size(); ++role) {
    if (plan.regions[role].minimum_entries == 0) {
      continue;
    }
    const auto region = workspace.regions[role];
    bad = workspace;
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
void Shapes(TestContext& test, const asc::ReferenceLapackProvider& provider,
            asc::DenseBlasTriangle triangle,
            const std::array<asc::DenseBlasLayout, 4>& layouts, char mode) {
  for (int field = 0; field < 10; ++field) {
    Fixture<T> data(3, 1, 2, triangle, layouts, mode);
    const auto plan = Take(data.Query(provider));
    WorkspaceStorage<T> storage(plan);
    if (field == 0) {
      --data.data.af.n;
    } else if (field == 1) {
      --data.data.af.kd;
    } else if (field == 2) {
      data.data.af.triangle = triangle == kUpper ? kLower : kUpper;
    } else if (field == 3) {
      --data.data.b.n;
    } else if (field == 4) {
      --data.data.x.n;
    } else if (field == 5) {
      --data.data.x.count;
    } else if (field == 6) {
      --data.data.ferr_count;
    } else if (field == 7) {
      --data.data.berr_count;
    } else if (field == 8) {
      data.data.ferr_stride = 2;
    } else {
      data.data.berr_stride = 2;
    }
    ASC_DENSE_TEST_EQ(
        test,
        WithoutAllocation(test, [&] { return data.Query(provider); })
            .status()
            .code(),
        asc::ErrorCode::kShape);
    Rejected(test, provider, data, plan, storage.View(),
             asc::ErrorCode::kShape);
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
  const std::array<asc::MutableMemoryView, 4> metadata{
      {{const_cast<asc::ReferenceLapackProvider*>(&provider), sizeof(provider),
        kHost},
       {const_cast<asc::LapackWorkspacePlan*>(&plan), sizeof(plan), kHost},
       {&bad, sizeof(bad), kHost},
       {&report, sizeof(report), kHost}}};
  const auto a = data.data.a.values;
  const auto af = data.data.af.values;
  const auto b = data.data.b.values;
  const auto x = data.data.x.values;
  const auto ferr = data.data.ferr;
  const auto berr = data.data.berr;
  for (const auto object : metadata) {
    bad.regions[kUnused] = object;
    ResetFault();
    ASC_DENSE_TEST_EQ(
        test,
        WithoutAllocation(
            test, [&] { return data.Execute(provider, plan, bad, report); })
            .code(),
        asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 73);
  }
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b, data.data.b.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(x, data.data.x.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(ferr, data.data.ferr));
  ASC_DENSE_TEST_CHECK(test, SameBytes(berr, data.data.berr));
}

template <typename T>
void ScaleFailures(TestContext& test,
                   const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle,
                   const std::array<asc::DenseBlasLayout, 4>& layouts) {
  using Real = asc::DenseBlasRealType<T>;
  for (const char mode : {'E', 'F'}) {
    Fixture<T> data(3, 1, 2, triangle, layouts, mode);
    data.equilibration = asc::LapackCholeskyEquilibration::kDiagonal;
    std::fill(data.scales.begin(), data.scales.end(), Real{1});
    const auto plan = Take(data.Query(provider));
    WorkspaceStorage<T> storage(plan);
    --data.scale_count;
    Rejected(test, provider, data, plan, storage.View(),
             asc::ErrorCode::kShape);
    ++data.scale_count;
    data.scales.resize(8, Real{1});
    data.scale_stride = 2;
    Rejected(test, provider, data, plan, storage.View(),
             asc::ErrorCode::kShape);
    data.scale_stride = 1;
    if (mode == 'F') {
      for (const Real invalid :
           {Real{0}, Real{-1}, std::numeric_limits<Real>::infinity(),
            std::numeric_limits<Real>::quiet_NaN()}) {
        data.scales[2] = invalid;
        ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                                     return data.Query(provider);
                                   }).ok());
        Rejected(test, provider, data, plan, storage.View(),
                 asc::ErrorCode::kInvalidArgument);
      }
      data.scales[2] = Real{1};
      // Intentionally malformed enum, matching the repository's negative
      // reference-Cholesky enum test; the rejection assertion remains active.
      // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
      data.equilibration = static_cast<asc::LapackCholeskyEquilibration>(77);
      Rejected(test, provider, data, plan, storage.View(),
               asc::ErrorCode::kInvalidArgument);
    }
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      for (const char mode : {'N', 'E', 'F'}) {
        Fixture<T> data(3, 1, 2, triangle, layouts, mode);
        const auto plan = Take(data.Query(provider));
        WorkspaceStorage<T> storage(plan);
        Freshness(test, provider, data, plan, storage.View());
        WorkFailures(test, provider, data, plan, storage.View());
        MetadataAlias(test, provider, data, plan, storage.View());
        Shapes<T>(test, provider, triangle, layouts, mode);
        Placement(test, provider, data, plan, storage.View());
      }
      ScaleFailures<T>(test, provider, triangle, layouts);
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
