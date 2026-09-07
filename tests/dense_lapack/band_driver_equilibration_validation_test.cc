#include <algorithm>
#include <array>
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
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band_driver.h"
#include "asc/dense/providers/lapack_cholesky_band_equilibration.h"
#include "band_cholesky_test_support.h"
#include "band_expert_fault_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)
using asc_band_expert_test::FaultArgumentsValid;
using asc_band_expert_test::FaultCalls;
using asc_band_expert_test::ResetFault;

template <typename T>
struct Fixture {
  using Real = asc::DenseBlasRealType<T>;
  BandData<T> band;
  RhsData<T> rhs;
  std::vector<Real> scales;
  asc::LapackBandEquilibrationStatistics<Real> statistics{Real{-17}, Real{-19}};

  Fixture(asc::DenseBlasTriangle triangle, asc::DenseBlasLayout layout,
          asc::DenseBlasLayout rhs_layout)
      : band(3, 1, triangle, layout),
        rhs(band, 2, rhs_layout),
        scales(5, Real{-23}) {}
  asc::DenseBlasVectorView<Real> Scales(asc::MemorySpace space = kHost) {
    return Take(asc::DenseBlasVectorView<Real>::Create(
        scales.data() + 1, band.n, 1,
        {scales.data(), scales.size() * sizeof(Real), space}));
  }
  asc::Result<asc::LapackWorkspacePlan> Query(
      const asc::ReferenceLapackProvider& provider, bool equilibration,
      asc::MemorySpace band_space = kHost,
      asc::MemorySpace output_space = kHost) {
    if (equilibration) {
      return asc::QueryPbequWorkspace(provider, band.ConstView(band_space),
                                      Scales(output_space), statistics);
    }
    return asc::QueryPbsvWorkspace(provider, band.View(band_space),
                                   rhs.View(output_space));
  }
  asc::Status Execute(const asc::ReferenceLapackProvider& provider,
                      bool equilibration, const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report,
                      asc::MemorySpace band_space = kHost,
                      asc::MemorySpace output_space = kHost) {
    if (equilibration) {
      return asc::Pbequ(provider, band.ConstView(band_space),
                        Scales(output_space), statistics, plan, workspace,
                        report);
    }
    return asc::Pbsv(provider, band.View(band_space), rhs.View(output_space),
                     plan, workspace, report);
  }
};

template <typename T>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              bool equilibration, Fixture<T>& data,
              const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace, asc::ErrorCode code,
              asc::MemorySpace band_space = kHost,
              asc::MemorySpace output_space = kHost) {
  const auto matrix = data.band.values;
  const auto rhs = data.rhs.values;
  const auto scales = data.scales;
  const auto statistics = data.statistics;
  std::array<std::vector<std::byte>, 8> bytes;
  for (std::size_t i = 0; i < workspace.regions.size(); ++i) {
    const auto region = workspace.regions[i];
    if (region.data() != nullptr && region.size() != 0) {
      const auto* first = static_cast<const std::byte*>(region.data());
      bytes[i].assign(first, first + region.size());
    }
  }
  asc::LapackReport report;
  report.native_info = 73;
  report.called_provider = true;
  ResetFault(0);
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, equilibration, plan, workspace, report,
                        band_space, output_space);
  });
  ASC_DENSE_TEST_EQ(test, status.code(), code);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(test, SameBytes(matrix, data.band.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(rhs, data.rhs.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(scales, data.scales));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(statistics.scale_condition,
                                             data.statistics.scale_condition));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(statistics.absolute_maximum,
                                             data.statistics.absolute_maximum));
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (!bytes[i].empty()) {
      const auto* first =
          static_cast<const std::byte*>(workspace.regions[i].data());
      ASC_DENSE_TEST_CHECK(test,
                           std::equal(bytes[i].begin(), bytes[i].end(), first));
    }
  }
}

template <typename T>
void Placement(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool equilibration, Fixture<T>& data,
               const asc::LapackWorkspacePlan& plan,
               const asc::LapackWorkspace& workspace) {
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    if (space == asc::MemorySpace::kPinnedHost) {
      ASC_DENSE_TEST_EQ(
          test, data.Query(provider, equilibration, space).status().code(),
          asc::ErrorCode::kMemoryAccess);
      Rejected(test, provider, equilibration, data, plan, workspace,
               asc::ErrorCode::kMemoryAccess, space);
    }
    ASC_DENSE_TEST_EQ(
        test, data.Query(provider, equilibration, kHost, space).status().code(),
        asc::ErrorCode::kMemoryAccess);
    Rejected(test, provider, equilibration, data, plan, workspace,
             asc::ErrorCode::kMemoryAccess, kHost, space);
    for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
      alignas(64) std::array<T, 64> scratch{};
      auto bad = workspace;
      bad.regions[role] = {scratch.data(), sizeof(scratch), space};
      Rejected(test, provider, equilibration, data, plan, bad,
               asc::ErrorCode::kMemoryAccess);
    }
  }
  auto good = workspace;
  good.regions[0] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
  ResetFault(0);
  asc::LapackReport report;
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return data.Execute(provider, equilibration,
                                                   plan, good, report);
                             }).ok());
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
}

template <typename T>
void Freshness(TestContext& test, const asc::ReferenceLapackProvider& provider,
               bool equilibration, Fixture<T>& data,
               const asc::LapackWorkspacePlan& plan,
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
      Rejected(test, provider, equilibration, data, stale, workspace,
               asc::ErrorCode::kInvalidState);
    }
  }
  auto stale = plan;
  --stale.total_byte_limit;
  Rejected(test, provider, equilibration, data, stale, workspace,
           asc::ErrorCode::kInvalidState);
  stale = plan;
  stale.identity = Take(asc::LapackPlanIdentity::Create(
      "not_this_routine", plan.identity.scalar(), {}, {}, provider.identity()));
  Rejected(test, provider, equilibration, data, stale, workspace,
           asc::ErrorCode::kInvalidState);
}

template <typename T>
void Aliases(TestContext& test, const asc::ReferenceLapackProvider& provider,
             bool equilibration, Fixture<T>& data,
             const asc::LapackWorkspacePlan& plan,
             const asc::LapackWorkspace& workspace) {
  auto bad = workspace;
  bad.regions[0] = {nullptr, sizeof(T), kHost};
  Rejected(test, provider, equilibration, data, plan, bad,
           asc::ErrorCode::kMemoryAccess);
  bad.regions[0] = {data.band.values.data() + 1, sizeof(T), kHost};
  Rejected(test, provider, equilibration, data, plan, bad,
           asc::ErrorCode::kInvalidArgument);
  std::array<T, 4> scratch{};
  bad.regions[0] = {scratch.data(), 2 * sizeof(T), kHost};
  bad.regions[1] = {scratch.data() + 1, 2 * sizeof(T), kHost};
  Rejected(test, provider, equilibration, data, plan, bad,
           asc::ErrorCode::kInvalidArgument);
  if (plan.regions[kLayout].minimum_entries > 0) {
    bad = workspace;
    const auto region = bad.regions[kLayout];
    bad.regions[kLayout] = {region.data(), region.size() - 1, kHost};
    Rejected(test, provider, equilibration, data, plan, bad,
             asc::ErrorCode::kInvalidArgument);
    bad.regions[kLayout] = {static_cast<std::byte*>(region.data()) + 1,
                            region.size(), kHost};
    Rejected(test, provider, equilibration, data, plan, bad,
             asc::ErrorCode::kInvalidArgument);
  }
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 73;
  bad = workspace;
  bad.regions[0] = {&report, sizeof(report), kHost};
  ResetFault(0);
  const auto before = data.band.values;
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return data.Execute(provider,
                                                            equilibration, plan,
                                                            bad, report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 73);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.band.values));
}

template <typename T>
void Fault(TestContext& test, const asc::ReferenceLapackProvider& provider,
           bool equilibration, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
           std::int64_t info) {
  Fixture<T> data(triangle, layout, rhs_layout);
  const auto plan = Take(data.Query(provider, equilibration));
  Storage<T> storage(plan);
  auto workspace = storage.View();
  const auto before = data.band.values;
  const auto rhs_before = data.rhs.values;
  asc::LapackReport report;
  ResetFault(info, !equilibration && layout == kRow);
  const auto status = WithoutAllocation(test, [&] {
    return data.Execute(provider, equilibration, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(73), info);
  ASC_DENSE_TEST_EQ(test, status.ok(), info == 0);
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
    ASC_DENSE_TEST_EQ(
        test, status.code(),
        info <= 3 ? asc::ErrorCode::kNumerical : asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      info <= 3 ? asc::LapackOutputValidity::kDocumentedPartial
                                : asc::LapackOutputValidity::kUnusable);
  }
  if (equilibration) {
    ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.band.values));
    ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, data.rhs.values));
    ASC_DENSE_TEST_CHECK(
        test, SameScalarBytes(data.scales[1], asc::DenseBlasRealType<T>{-139}));
  } else {
    const bool publish = layout == kColumn || (info >= 0 && info <= 3);
    if (publish) {
      ASC_DENSE_TEST_CHECK(
          test, SameScalarBytes(data.band.values[data.band.Index(0, 0)],
                                Value<T>(-131)));
    } else {
      ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.band.values));
    }
    if (info == 0) {
      ASC_DENSE_TEST_CHECK(
          test, SameScalarBytes(data.rhs.values[data.rhs.Index(0, 0)],
                                Value<T>(-137)));
    } else {
      ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, data.rhs.values));
    }
    data.band.CheckPadding(test, before);
  }
  storage.Check(test);
}

template <typename T>
void ScaleShapes(TestContext& test,
                 const asc::ReferenceLapackProvider& provider,
                 Fixture<T>& data) {
  using Real = asc::DenseBlasRealType<T>;
  std::array<Real, 7> extra{};
  for (const asc::extent_t count : {2, 3}) {
    const auto output = Take(asc::DenseBlasVectorView<Real>::Create(
        extra.data(), count, count == 3 ? 2 : 1,
        {extra.data(), sizeof(extra), kHost}));
    const auto code =
        count == 2 ? asc::ErrorCode::kShape : asc::ErrorCode::kInvalidArgument;
    const auto query = WithoutAllocation(test, [&] {
      return asc::QueryPbequWorkspace(provider, data.band.ConstView(), output,
                                      data.statistics);
    });
    ASC_DENSE_TEST_EQ(test, query.status().code(), code);
    const auto valid_plan = Take(data.Query(provider, true));
    Storage<T> storage(valid_plan);
    asc::LapackReport report;
    report.native_info = 73;
    ResetFault(0);
    ASC_DENSE_TEST_EQ(test,
                      WithoutAllocation(test,
                                        [&] {
                                          return asc::Pbequ(
                                              provider, data.band.ConstView(),
                                              output, data.statistics,
                                              valid_plan, storage.View(),
                                              report);
                                        })
                          .code(),
                      code);
    ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider);
    ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
    ASC_DENSE_TEST_CHECK(test,
                         std::all_of(extra.begin(), extra.end(),
                                     [](Real value) { return value == 0; }));
    storage.Check(test);
  }
}

template <typename T>
void Shapes(TestContext& test, const asc::ReferenceLapackProvider& provider,
            Fixture<T>& data) {
  const auto before = data.band.values;
  const auto rhs_before = data.rhs.values;
  const auto scale_before = data.scales;
  const auto statistics_before = data.statistics;
  ScaleShapes(test, provider, data);
  const auto bad_rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      data.rhs.values.data() + 1, 2, 1, data.rhs.layout, data.rhs.ld,
      {data.rhs.values.data(), data.rhs.values.size() * sizeof(T), kHost}));
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::QueryPbsvWorkspace(
                                            provider, data.band.View(),
                                            bad_rhs);
                                      })
                        .status()
                        .code(),
                    asc::ErrorCode::kShape);
  const auto valid_plan = Take(data.Query(provider, false));
  Storage<T> storage(valid_plan);
  asc::LapackReport report;
  report.native_info = 73;
  ResetFault(0);
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::Pbsv(
                                            provider, data.band.View(), bad_rhs,
                                            valid_plan, storage.View(), report);
                                      })
                        .code(),
                    asc::ErrorCode::kShape);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  const auto overlapping_rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      data.band.values.data() + 1, 3, 1, kColumn, 3,
      {data.band.values.data(), data.band.values.size() * sizeof(T), kHost}));
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::QueryPbsvWorkspace(
                                            provider, data.band.View(),
                                            overlapping_rhs);
                                      })
                        .status()
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test,
                    WithoutAllocation(test,
                                      [&] {
                                        return asc::Pbsv(
                                            provider, data.band.View(),
                                            overlapping_rhs, valid_plan,
                                            storage.View(), report);
                                      })
                        .code(),
                    asc::ErrorCode::kInvalidArgument);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before, data.band.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, data.rhs.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(scale_before, data.scales));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(statistics_before.scale_condition,
                                             data.statistics.scale_condition));
  ASC_DENSE_TEST_CHECK(test, SameScalarBytes(statistics_before.absolute_maximum,
                                             data.statistics.absolute_maximum));
  storage.Check(test);
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const bool equilibration : {false, true}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kColumn, kRow}) {
        for (const auto rhs_layout : {kColumn, kRow}) {
          Fixture<T> data(triangle, layout, rhs_layout);
          Shapes(test, provider, data);
          const auto plan = Take(data.Query(provider, equilibration));
          Storage<T> storage(plan);
          const auto workspace = storage.View();
          Placement(test, provider, equilibration, data, plan, workspace);
          Freshness(test, provider, equilibration, data, plan, workspace);
          Aliases(test, provider, equilibration, data, plan, workspace);
          for (const auto info : std::array<std::int64_t, 5>{
                   0, -2, 3, 4, std::numeric_limits<lapack_int>::min()}) {
            Fault<T>(test, provider, equilibration, triangle, layout,
                     rhs_layout, info);
          }
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
