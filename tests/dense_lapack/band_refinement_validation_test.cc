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
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_refinement.h"
#include "band_cholesky_test_support.h"
#include "band_estimation_test_support.h"
#include "band_refinement_fault_support.h"
#include "band_refinement_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_estimation_test::WorkspaceStorage;
using asc_band_refinement_test::FaultArgumentsValid;
using asc_band_refinement_test::FaultCalls;
using asc_band_refinement_test::Fixture;
using asc_band_refinement_test::kHostSpaces;
using asc_band_refinement_test::ResetFault;

template <typename T>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Fixture<T>& data, const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace, asc::ErrorCode code,
              const std::array<asc::MemorySpace, 6>& spaces = kHostSpaces) {
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto b = data.b.values;
  const auto x = data.x.values;
  const auto ferr = data.ferr;
  const auto berr = data.berr;
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
  ResetFault(0, 0.5, 0.5, false);
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, plan, workspace, report, spaces);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), code);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b, data.b.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(x, data.x.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(ferr, data.ferr));
  ASC_DENSE_TEST_CHECK(test, SameBytes(berr, data.berr));
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
    for (std::size_t operand = 0; operand < 6; ++operand) {
      if (operand < 2 && space != asc::MemorySpace::kPinnedHost) {
        continue;
      }
      auto spaces = kHostSpaces;
      spaces[operand] = space;
      ASC_DENSE_TEST_EQ(
          test,
          WithoutAllocation(test, [&] { return data.Query(provider, spaces); })
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
  constexpr auto kUnused =
      static_cast<std::size_t>(asc::LapackWorkspaceKind::kLogical);
  auto good = workspace;
  good.regions[kUnused] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
  ResetFault(0, 0.5, 0.5, data.a.layout == kRow);
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
      "not_pbrfs", plan.identity.scalar(), {}, {}, provider.identity()));
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
  for (void* operand : {static_cast<void*>(data.a.values.data() + 1),
                        static_cast<void*>(data.af.values.data() + 1),
                        static_cast<void*>(data.b.values.data() + 1),
                        static_cast<void*>(data.x.values.data() + 1),
                        static_cast<void*>(data.ferr.data() + 1),
                        static_cast<void*>(data.berr.data() + 1)}) {
    bad = workspace;
    bad.regions[kUnused] = {operand, sizeof(asc::DenseBlasRealType<T>), kHost};
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
            const std::array<asc::DenseBlasLayout, 4>& layouts) {
  for (int field = 0; field < 10; ++field) {
    Fixture<T> data(3, 1, 2, triangle, layouts);
    const auto plan = Take(data.Query(provider));
    WorkspaceStorage<T> storage(plan);
    if (field == 0) {
      --data.af.n;
    } else if (field == 1) {
      --data.af.kd;
    } else if (field == 2) {
      data.af.triangle = triangle == kUpper ? kLower : kUpper;
    } else if (field == 3) {
      --data.b.n;
    } else if (field == 4) {
      --data.x.n;
    } else if (field == 5) {
      --data.x.count;
    } else if (field == 6) {
      --data.ferr_count;
    } else if (field == 7) {
      --data.berr_count;
    } else if (field == 8) {
      data.ferr_stride = 2;
    } else {
      data.berr_stride = 2;
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
  bad.regions[kUnused] = {&report, sizeof(report), kHost};
  ResetFault(0, 0.5, 0.5, false);
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto b = data.b.values;
  const auto x = data.x.values;
  const auto ferr = data.ferr;
  const auto berr = data.berr;
  ASC_DENSE_TEST_EQ(
      test,
      WithoutAllocation(
          test, [&] { return data.Execute(provider, plan, bad, report); })
          .code(),
      asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 73);
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b, data.b.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(x, data.x.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(ferr, data.ferr));
  ASC_DENSE_TEST_CHECK(test, SameBytes(berr, data.berr));
}

template <typename T>
void OperandAlias(TestContext& test,
                  const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
                  std::size_t first, std::size_t second) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<std::array<T, 32>, 6> backing{};
  std::array<T*, 6> pointers{};
  for (std::size_t i = 0; i < backing.size(); ++i) {
    backing[i].fill(T{13});
    pointers[i] = backing[i].data();
  }
  pointers[second] = pointers[first];
  const auto band = [&](std::size_t i) {
    return Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
        pointers[i], 3, 1, triangle, layout, 4,
        {pointers[i], sizeof(backing[i]), kHost}));
  };
  const auto rhs = Take(asc::DenseBlasMatrixView<const T>::Create(
      pointers[2], 3, 2, layout, 4, {pointers[2], sizeof(backing[2]), kHost}));
  const auto x = Take(asc::DenseBlasMatrixView<T>::Create(
      pointers[3], 3, 2, layout, 4, {pointers[3], sizeof(backing[3]), kHost}));
  const auto vector = [&](std::size_t i) {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        reinterpret_cast<Real*>(pointers[i]), 2, 1,
        {pointers[i], sizeof(backing[i]), kHost}));
  };
  const auto a = band(0);
  const auto af = band(1);
  const auto ferr = vector(4);
  const auto berr = vector(5);
  const auto before = backing;
  const auto query = WithoutAllocation(test, [&] {
    return asc::QueryPbrfsWorkspace(provider, a, af, rhs, x, ferr, berr);
  });
  ASC_DENSE_TEST_EQ(test, query.status().code(),
                    asc::ErrorCode::kInvalidArgument);
  Fixture<T> good(3, 1, 2, triangle, {layout, layout, layout, layout});
  const auto plan = Take(good.Query(provider));
  WorkspaceStorage<T> storage(plan);
  asc::LapackReport report;
  ResetFault(0, 0.5, 0.5, false);
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::Pbrfs(
                                            provider, a, af, rhs, x, ferr, berr,
                                            plan, storage.View(), report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(
      test, !report.called_provider && !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_EQ(test, before, backing);
  storage.Check(test);
}

void CheckInfo(TestContext& test, const asc::Status& status,
               const asc::LapackReport& report, std::int64_t info,
               bool quality) {
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(73), info);
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_EQ(test, status.ok(), info == 0 && !quality);
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
  } else if (quality) {
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kNumerical);
    ASC_DENSE_TEST_EQ(test, report.outcome,
                      asc::LapackOutcome::kAccuracyWarning);
  } else {
    ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kSuccess);
  }
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    info != 0 ? asc::LapackOutputValidity::kUnusable
                    : quality ? asc::LapackOutputValidity::kDocumentedPartial
                              : asc::LapackOutputValidity::kComplete);
}

template <typename T>
void Fault(TestContext& test, const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle,
           const std::array<asc::DenseBlasLayout, 4>& layouts,
           std::int64_t info, double ferr, double berr) {
  using Real = asc::DenseBlasRealType<T>;
  Fixture<T> data(3, 1, 2, triangle, layouts);
  const auto plan = Take(data.Query(provider));
  WorkspaceStorage<T> storage(plan);
  const auto a = data.a.values;
  const auto af = data.af.values;
  const auto b = data.b.values;
  const auto x = data.x.values;
  ResetFault(info, ferr, berr, layouts[0] == kRow);
  asc::LapackReport report;
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, plan, storage.View(), report);
  });
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
  CheckInfo(
      test, status, report, info,
      !std::isfinite(ferr) || ferr < 0 || !std::isfinite(berr) || berr < 0);
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  ASC_DENSE_TEST_CHECK(test, SameBytes(a, data.a.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(af, data.af.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(b, data.b.values));
  for (asc::extent_t j = 0; j < data.x.count; ++j) {
    ASC_DENSE_TEST_CHECK(
        test, SameScalarBytes(data.ferr[static_cast<std::size_t>(j + 1)],
                              static_cast<Real>(ferr)));
    ASC_DENSE_TEST_CHECK(
        test, SameScalarBytes(data.berr[static_cast<std::size_t>(j + 1)],
                              static_cast<Real>(berr)));
    for (asc::extent_t i = 0; i < data.x.n; ++i) {
      const auto at = data.x.Index(i, j);
      ASC_DENSE_TEST_EQ(test, data.x.values[at],
                        info == 0 || layouts[3] == kColumn ? T{-347} : x[at]);
    }
  }
  ASC_DENSE_TEST_EQ(test, data.ferr.front(), Real{-317});
  ASC_DENSE_TEST_EQ(test, data.ferr.back(), Real{-317});
  ASC_DENSE_TEST_EQ(test, data.berr.front(), Real{-331});
  ASC_DENSE_TEST_EQ(test, data.berr.back(), Real{-331});
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (int bits = 0; bits < 16; ++bits) {
      const std::array layouts{
          bits & 1 ? kRow : kColumn, bits & 2 ? kRow : kColumn,
          bits & 4 ? kRow : kColumn, bits & 8 ? kRow : kColumn};
      Fixture<T> data(3, 1, 2, triangle, layouts);
      const auto plan = Take(data.Query(provider));
      WorkspaceStorage<T> storage(plan);
      Placement(test, provider, data, plan, storage.View());
      Freshness(test, provider, data, plan, storage.View());
      WorkFailures(test, provider, data, plan, storage.View());
      MetadataAlias(test, provider, data, plan, storage.View());
      Shapes<T>(test, provider, triangle, layouts);
      for (const std::int64_t info :
           {std::int64_t{0}, std::int64_t{-6}, std::int64_t{1},
            static_cast<std::int64_t>(
                std::numeric_limits<lapack_int>::min())}) {
        Fault<T>(test, provider, triangle, layouts, info, 0.5, 0.25);
      }
      for (const double raw : {-1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
        Fault<T>(test, provider, triangle, layouts, 0, raw, 0.25);
        Fault<T>(test, provider, triangle, layouts, 0, 0.5, raw);
      }
    }
    for (const auto layout : {kColumn, kRow}) {
      for (std::size_t first = 0; first < 6; ++first) {
        for (std::size_t second = first + 1; second < 6; ++second) {
          OperandAlias<T>(test, provider, triangle, layout, first, second);
        }
      }
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
