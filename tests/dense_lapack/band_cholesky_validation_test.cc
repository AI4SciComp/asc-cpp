#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_band.h"
#include "band_cholesky_fault_support.h"
#include "band_cholesky_test_support.h"
#include "installed_lu/normal_return_guard.h"

namespace {
using namespace asc_band_test;  // NOLINT(google-build-using-namespace)

template <typename T>
asc::Result<asc::LapackWorkspacePlan> Query(
    const asc::ReferenceLapackProvider& provider, int routine,
    BandData<T>& band, RhsData<T>& rhs, asc::MemorySpace band_space = kHost,
    asc::MemorySpace rhs_space = kHost) {
  if (routine == 0) {
    return asc::QueryPbtrfWorkspace(provider, band.View(band_space));
  }
  if (routine == 1) {
    return asc::QueryPbtf2Workspace(provider, band.View(band_space));
  }
  return asc::QueryPbtrsWorkspace(provider, band.ConstView(band_space),
                                  rhs.View(rhs_space));
}
template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider, int routine,
                    BandData<T>& band, RhsData<T>& rhs,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report,
                    asc::MemorySpace band_space = kHost,
                    asc::MemorySpace rhs_space = kHost) {
  if (routine == 0) {
    return asc::Pbtrf(provider, band.View(band_space), plan, workspace, report);
  }
  if (routine == 1) {
    return asc::Pbtf2(provider, band.View(band_space), plan, workspace, report);
  }
  return asc::Pbtrs(provider, band.ConstView(band_space), rhs.View(rhs_space),
                    plan, workspace, report);
}
template <typename T>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              int routine, BandData<T>& band, RhsData<T>& rhs,
              const asc::LapackWorkspacePlan& plan,
              const asc::LapackWorkspace& workspace, asc::ErrorCode code,
              asc::MemorySpace band_space = kHost,
              asc::MemorySpace rhs_space = kHost) {
  const auto before_band = band.values;
  const auto before_rhs = rhs.values;
  std::array<std::vector<std::byte>, 8> before_workspace;
  for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
    const auto region = workspace.regions[role];
    if (region.data() != nullptr && region.size() != 0) {
      const auto* begin = static_cast<const std::byte*>(region.data());
      before_workspace[role].assign(begin, begin + region.size());
    }
  }
  asc::LapackReport report;
  report.called_provider = true;
  report.native_info = 117;
  ResetFault(0);
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, band, rhs, plan, workspace, report,
                   band_space, rhs_space);
  });
  if (status.code() != code) {
    std::fprintf(stderr, "Rejection route=%d got=%d expected=%d\n", routine,
                 static_cast<int>(status.code()), static_cast<int>(code));
  }
  ASC_DENSE_TEST_EQ(test, status.code(), code);
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider);
  ASC_DENSE_TEST_CHECK(test, !report.native_info.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.native_argument.has_value());
  ASC_DENSE_TEST_CHECK(test, !report.diagnostic_index.has_value());
  ASC_DENSE_TEST_EQ(test, report.outcome, asc::LapackOutcome::kNotRun);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  ASC_DENSE_TEST_CHECK(test, SameBytes(before_band, band.values));
  ASC_DENSE_TEST_CHECK(test, SameBytes(before_rhs, rhs.values));
  for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
    const auto& bytes = before_workspace[role];
    if (!bytes.empty()) {
      const auto* begin =
          static_cast<const std::byte*>(workspace.regions[role].data());
      ASC_DENSE_TEST_CHECK(test, std::equal(bytes.begin(), bytes.end(), begin));
    }
  }
}

template <typename T>
void ValidatePlacement(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       int routine, BandData<T>& band, RhsData<T>& rhs,
                       const asc::LapackWorkspacePlan& plan,
                       const asc::LapackWorkspace& workspace) {
  const auto triangle = band.triangle;
  const auto layout = band.layout;
  for (const auto space :
       {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
        asc::MemorySpace::kManaged}) {
    if (space == asc::MemorySpace::kPinnedHost) {
      ASC_DENSE_TEST_EQ(
          test,
          WithoutAllocation(
              test, [&] { return Query(provider, routine, band, rhs, space); })
              .status()
              .code(),
          asc::ErrorCode::kMemoryAccess);
      Rejected(test, provider, routine, band, rhs, plan, workspace,
               asc::ErrorCode::kMemoryAccess, space);
    } else {
      // The existing checked band descriptor already excludes device/managed
      // operands. Prove that gate without fabricating an invalid descriptor.
      const auto rejected = WithoutAllocation(test, [&] {
        return asc::LapackPositiveDefiniteBandView<T>::Create(
            band.values.data() + 1, band.n, band.kd, triangle, layout, band.ld,
            {band.values.data(), band.values.size() * sizeof(T), space});
      });
      ASC_DENSE_TEST_EQ(test, rejected.status().code(),
                        asc::ErrorCode::kInvalidArgument);
    }
    if (routine == 2) {
      ASC_DENSE_TEST_EQ(
          test,
          WithoutAllocation(
              test,
              [&] { return Query(provider, routine, band, rhs, kHost, space); })
              .status()
              .code(),
          asc::ErrorCode::kMemoryAccess);
      Rejected(test, provider, routine, band, rhs, plan, workspace,
               asc::ErrorCode::kMemoryAccess, kHost, space);
    }
    // Every nonempty region is context checked, including all unused roles.
    for (std::size_t role = 0; role < workspace.regions.size(); ++role) {
      alignas(64) std::array<T, 64> scratch{};
      auto bad = workspace;
      bad.regions[role] = {scratch.data(), sizeof(scratch), space};
      const auto before = scratch;
      Rejected(test, provider, routine, band, rhs, plan, bad,
               asc::ErrorCode::kMemoryAccess);
      ASC_DENSE_TEST_EQ(test, scratch, before);
    }
  }
  // Generic zero-byte pinned compatibility is unchanged for an unused role.
  {
    auto good = workspace;
    good.regions[0] = {nullptr, 0, asc::MemorySpace::kPinnedHost};
    asc::LapackReport report;
    ResetFault(0);
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, band, rhs, plan, good, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
    ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
  }
}

template <typename T>
void ValidateFreshness(TestContext& test,
                       const asc::ReferenceLapackProvider& provider,
                       int routine, BandData<T>& band, RhsData<T>& rhs,
                       const asc::LapackWorkspacePlan& plan,
                       const asc::LapackWorkspace& workspace) {
  const auto triangle = band.triangle;
  const auto layout = band.layout;
  const auto rhs_layout = rhs.layout;
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
      Rejected(test, provider, routine, band, rhs, stale, workspace,
               asc::ErrorCode::kInvalidState);
    }
  }
  {
    auto stale = plan;
    --stale.total_byte_limit;
    Rejected(test, provider, routine, band, rhs, stale, workspace,
             asc::ErrorCode::kInvalidState);
  }
  {
    auto stale = plan;
    stale.identity = Take(asc::LapackPlanIdentity::Create(
        "wrong_routine", plan.identity.scalar(), {}, {}, provider.identity()));
    Rejected(test, provider, routine, band, rhs, stale, workspace,
             asc::ErrorCode::kInvalidState);
  }
  for (const int changed : {0, 1, 2, 3}) {
    auto other_triangle = triangle;
    auto other_layout = layout;
    if (changed == 2) {
      other_triangle = triangle == kUpper ? kLower : kUpper;
    }
    if (changed == 3) {
      other_layout = layout == kColumn ? kRow : kColumn;
    }
    BandData<T> other(changed == 0 ? 4 : 3, changed == 1 ? 2 : 1,
                      other_triangle, other_layout);
    RhsData<T> other_rhs(other, 2, rhs_layout);
    Rejected(test, provider, routine, other, other_rhs, plan, workspace,
             asc::ErrorCode::kInvalidState);
  }
}

template <typename T>
void ValidateAliases(TestContext& test,
                     const asc::ReferenceLapackProvider& provider, int routine,
                     BandData<T>& band, RhsData<T>& rhs,
                     const asc::LapackWorkspacePlan& plan,
                     const asc::LapackWorkspace& workspace) {
  // Metadata aliases must be rejected before report initialization as well
  // as before numerical writes. The aliased objects are real live objects.
  {
    asc::LapackReport report;
    report.native_info = 117;
    report.called_provider = true;
    auto bad = workspace;
    bad.regions[0] = {&report, sizeof(report), kHost};
    const auto band_before = band.values;
    const auto rhs_before = rhs.values;
    ResetFault(0);
    const auto status = WithoutAllocation(test, [&] {
      return Execute(provider, routine, band, rhs, plan, bad, report);
    });
    ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kInvalidArgument);
    ASC_DENSE_TEST_EQ(test, FaultCalls(), 0U);
    ASC_DENSE_TEST_CHECK(test, report.called_provider);
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(0), 117);
    ASC_DENSE_TEST_CHECK(test, SameBytes(band_before, band.values));
    ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, rhs.values));
  }
  // All unused host regions still participate in null, overlap and operand
  // alias validation even though the foreign routines have no WORK parameter.
  {
    auto bad = workspace;
    bad.regions[0] = {nullptr, sizeof(T), kHost};
    Rejected(test, provider, routine, band, rhs, plan, bad,
             asc::ErrorCode::kMemoryAccess);
    bad.regions[0] = {band.values.data() + 1, sizeof(T), kHost};
    Rejected(test, provider, routine, band, rhs, plan, bad,
             asc::ErrorCode::kInvalidArgument);
    std::array<T, 4> separate{};
    bad.regions[0] = {separate.data(), sizeof(T) * 2, kHost};
    bad.regions[1] = {separate.data() + 1, sizeof(T) * 2, kHost};
    Rejected(test, provider, routine, band, rhs, plan, bad,
             asc::ErrorCode::kInvalidArgument);
  }
  if (plan.regions[kLayout].minimum_entries > 0) {
    auto bad = workspace;
    const auto region = workspace.regions[kLayout];
    bad.regions[kLayout] = {region.data(), region.size() - 1, kHost};
    Rejected(test, provider, routine, band, rhs, plan, bad,
             asc::ErrorCode::kInvalidArgument);
    bad.regions[kLayout] = {static_cast<std::byte*>(region.data()) + 1,
                            region.size(), kHost};
    Rejected(test, provider, routine, band, rhs, plan, bad,
             asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void Validation(TestContext& test, const asc::ReferenceLapackProvider& provider,
                int routine, asc::DenseBlasTriangle triangle,
                asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout) {
  BandData<T> band(3, 1, triangle, layout);
  RhsData<T> rhs(band, 2, rhs_layout);
  const auto plan = Take(Query(provider, routine, band, rhs));
  Storage<T> storage(plan);
  const auto workspace = storage.View();
  ValidatePlacement(test, provider, routine, band, rhs, plan, workspace);
  ValidateFreshness(test, provider, routine, band, rhs, plan, workspace);
  ValidateAliases(test, provider, routine, band, rhs, plan, workspace);
  storage.Check(test);
}

template <typename T>
void Fault(TestContext& test, const asc::ReferenceLapackProvider& provider,
           int routine, asc::DenseBlasTriangle triangle,
           asc::DenseBlasLayout layout, asc::DenseBlasLayout rhs_layout,
           std::int64_t info, InfoWrite info_write = InfoWrite::kComplete) {
  BandData<T> band(3, 1, triangle, layout);
  RhsData<T> rhs(band, 2, rhs_layout);
  const auto plan = Take(Query(provider, routine, band, rhs));
  Storage<T> storage(plan);
  auto workspace = storage.View();
  const auto before_band = band.values;
  const auto before_rhs = rhs.values;
  asc::LapackReport report;
  ResetFault(info, routine != 2 && layout == kRow, info_write);
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, routine, band, rhs, plan, workspace, report);
  });
  ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
  ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
  ASC_DENSE_TEST_CHECK(test, report.called_provider);
  ASC_DENSE_TEST_EQ(test, report.native_info.value_or(117), info);
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
    const bool partial_factor = routine != 2 && info <= band.n;
    ASC_DENSE_TEST_EQ(test, status.code(),
                      partial_factor ? asc::ErrorCode::kNumerical
                                     : asc::ErrorCode::kProvider);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      partial_factor
                          ? asc::LapackOutputValidity::kDocumentedPartial
                          : asc::LapackOutputValidity::kUnusable);
  }
  if (routine != 2) {
    const bool published = layout == kColumn || (info >= 0 && info <= band.n);
    if (published) {
      ASC_DENSE_TEST_EQ(test, band.values[band.Index(0, 0)], Value<T>(-131));
    } else {
      ASC_DENSE_TEST_CHECK(test, SameBytes(before_band, band.values));
    }
    ASC_DENSE_TEST_CHECK(test, SameBytes(before_rhs, rhs.values));
    band.CheckPadding(test, before_band);
  } else {
    if (rhs_layout == kColumn || info == 0) {
      ASC_DENSE_TEST_EQ(test, rhs.values[rhs.Index(0, 0)], Value<T>(-131));
    } else {
      ASC_DENSE_TEST_CHECK(test, SameBytes(before_rhs, rhs.values));
    }
    ASC_DENSE_TEST_CHECK(test, SameBytes(before_band, band.values));
  }
  storage.Check(test);
}

template <typename T>
void MissingEmptyInfo(TestContext& test,
                      const asc::ReferenceLapackProvider& provider) {
  for (const auto triangle : {kUpper, kLower}) {
    for (const auto layout : {kColumn, kRow}) {
      for (const int routine : {0, 1, 2}) {
        BandData<T> band(0, 0, triangle, layout);
        RhsData<T> rhs(band, 2, layout);
        const auto plan = Take(Query(provider, routine, band, rhs));
        Storage<T> storage(plan);
        const auto band_before = band.values;
        const auto rhs_before = rhs.values;
        asc::LapackReport report;
        ResetFault(0, false, InfoWrite::kOmitted);
        const auto status = WithoutAllocation(test, [&] {
          return Execute(provider, routine, band, rhs, plan, storage.View(),
                         report);
        });
        ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
        ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
        ASC_DENSE_TEST_CHECK(test, report.called_provider);
        ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kProvider);
        ASC_DENSE_TEST_EQ(test, report.native_info,
                          std::numeric_limits<lapack_int>::min());
        ASC_DENSE_TEST_EQ(test, report.output_validity,
                          asc::LapackOutputValidity::kUnusable);
        ASC_DENSE_TEST_CHECK(test, SameBytes(band_before, band.values));
        ASC_DENSE_TEST_CHECK(test, SameBytes(rhs_before, rhs.values));
        storage.Check(test);
      }
    }
  }
}

template <typename T>
void Empty(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  const auto wide_stride =
      static_cast<asc::stride_t>(std::numeric_limits<std::int32_t>::max()) + 1;
  // Real zero-sized descriptors: no imaginary huge allocation/backing.
  const auto band = Take(asc::LapackPositiveDefiniteBandView<T>::Create(
      nullptr, 0, 0, kUpper, kRow, wide_stride, {nullptr, 0, kHost}));
  const auto factor = Take(asc::LapackPositiveDefiniteBandView<const T>::Create(
      nullptr, 0, 0, kUpper, kRow, wide_stride, {nullptr, 0, kHost}));
  const auto rhs = Take(asc::DenseBlasMatrixView<T>::Create(
      nullptr, 0, 2, kRow, wide_stride, {nullptr, 0, kHost}));
  for (const int routine : {0, 1, 2}) {
    const auto plan = Take(WithoutAllocation(test, [&] {
      if (routine == 0) {
        return asc::QueryPbtrfWorkspace(provider, band);
      }
      if (routine == 1) {
        return asc::QueryPbtf2Workspace(provider, band);
      }
      return asc::QueryPbtrsWorkspace(provider, factor, rhs);
    }));
    asc::LapackWorkspace workspace;
    asc::LapackReport report;
    ResetFault(0);
    const auto status = WithoutAllocation(test, [&] {
      if (routine == 0) {
        return asc::Pbtrf(provider, band, plan, workspace, report);
      }
      if (routine == 1) {
        return asc::Pbtf2(provider, band, plan, workspace, report);
      }
      return asc::Pbtrs(provider, factor, rhs, plan, workspace, report);
    });
    ASC_DENSE_TEST_CHECK(test, status.ok());
    ASC_DENSE_TEST_EQ(test, FaultCalls(), 1U);
    ASC_DENSE_TEST_CHECK(test, FaultArgumentsValid());
    ASC_DENSE_TEST_EQ(test, report.native_info.value_or(-1), 0);
  }
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  for (const int routine : {0, 1, 2}) {
    for (const auto triangle : {kUpper, kLower}) {
      for (const auto layout : {kColumn, kRow}) {
        for (const auto rhs_layout : {kColumn, kRow}) {
          Validation<T>(test, provider, routine, triangle, layout, rhs_layout);
          for (const auto info : std::array<std::int64_t, 5>{
                   0, -2, 3, 4, std::numeric_limits<lapack_int>::min()}) {
            Fault<T>(test, provider, routine, triangle, layout, rhs_layout,
                     info);
          }
          Fault<T>(test, provider, routine, triangle, layout, rhs_layout,
                   std::numeric_limits<lapack_int>::min(), InfoWrite::kOmitted);
          if constexpr (sizeof(lapack_int) > sizeof(std::int32_t)) {
            Fault<T>(test, provider, routine, triangle, layout, rhs_layout,
                     std::numeric_limits<lapack_int>::min(),
                     InfoWrite::kLowWordOnly);
          }
        }
      }
    }
  }
  Empty<T>(test, provider);
  MissingEmptyInfo<T>(test, provider);
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
