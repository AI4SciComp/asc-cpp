#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include "../dense/test_support.h"
#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "provider_operand_placement_test.h"
#include "provider_placement_routes.h"
#include "provider_placement_support.h"

namespace {
using asc_placement_test::Data;
using asc_placement_test::Execute;
using asc_placement_test::kNames;
using asc_placement_test::Prepare;
using asc_placement_test::Query;
using asc_placement_test::Route;
using asc_placement_test::Take;
using asc_placement_test::TestContext;
using asc_placement_test::WithoutAllocation;

template <typename T>
void CheckLuNumerics(TestContext& test, Route route, const Data<T>& data) {
  const auto& v = data.values;
  if (route == Route::kGetrf || route == Route::kGetrf2 ||
      route == Route::kGetf2 || route == Route::kGesv) {
    ASC_DENSE_TEST_EQ(test, v.a[v.Offset(0, 0, 0)], T{4});
    ASC_DENSE_TEST_EQ(test, v.a[v.Offset(0, 1, 1)], T{4});
    ASC_DENSE_TEST_EQ(test, v.a[v.Offset(0, 0, 1)], T{0});
    ASC_DENSE_TEST_EQ(test, v.a[v.Offset(0, 1, 0)], T{0});
    ASC_DENSE_TEST_EQ(test, v.pivots[1], 1);
    ASC_DENSE_TEST_EQ(test, v.pivots[2], 2);
  } else if (route == Route::kGetriQuery) {
    ASC_DENSE_TEST_EQ(test, v.af[v.Offset(1, 0, 0)], T{4});
    ASC_DENSE_TEST_EQ(test, v.af[v.Offset(1, 1, 1)], T{4});
    ASC_DENSE_TEST_EQ(test, v.pivots[1], 1);
    ASC_DENSE_TEST_EQ(test, v.pivots[2], 2);
  }
}

template <typename T>
void CheckNumerics(TestContext& test, Route route, const Data<T>& data) {
  const auto& v = data.values;
  if (route == Route::kGetri || route == Route::kPotri) {
    ASC_DENSE_TEST_EQ(test, v.af[v.Offset(1, 0, 0)], T{0.25});
    ASC_DENSE_TEST_EQ(test, v.af[v.Offset(1, 1, 1)], T{0.25});
  } else if (route == Route::kPotrf || route == Route::kPotrf2 ||
             route == Route::kPotf2 || route == Route::kPosv) {
    ASC_DENSE_TEST_EQ(test, v.a[v.Offset(0, 0, 0)], T{2});
    ASC_DENSE_TEST_EQ(test, v.a[v.Offset(0, 1, 1)], T{2});
  } else if (route == Route::kGecon) {
    ASC_DENSE_TEST_EQ(test, data.condition, 1);
  } else if (route == Route::kGeequ || route == Route::kGeequb) {
    ASC_DENSE_TEST_EQ(test, v.rows[1], 0.25);
    ASC_DENSE_TEST_EQ(test, v.rows[2], 0.25);
    ASC_DENSE_TEST_EQ(test, v.columns[1], 1);
    ASC_DENSE_TEST_EQ(test, v.columns[2], 1);
  }
  if (route == Route::kGetrs || route == Route::kGesv ||
      route == Route::kPotrs || route == Route::kPosv) {
    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 2; ++j) {
        ASC_DENSE_TEST_EQ(test, v.b[v.Offset(2, i, j)],
                          static_cast<T>(i + j + 1));
      }
    }
  }
  if (route == Route::kGerfs || route == Route::kGesvx ||
      route == Route::kGesvxEquilibrated || route == Route::kGesvxFactored) {
    for (int i = 0; i < 2; ++i) {
      ASC_DENSE_TEST_CHECK(test, std::isfinite(v.ferr[i + 1]) &&
                                     v.ferr[i + 1] >= 0 && v.berr[i + 1] == 0);
      for (int j = 0; j < 2; ++j) {
        ASC_DENSE_TEST_EQ(test, v.x[v.Offset(3, i, j)],
                          static_cast<T>(i + j + 1));
      }
    }
  }
}

template <typename T>
void CheckReportIdentity(TestContext& test,
                         const asc::ReferenceLapackProvider& provider,
                         Route route, const asc::LapackReport& report) {
  ASC_DENSE_TEST_EQ(test, report.provider, provider.identity());
  const std::string_view name(report.routine.data());
  ASC_DENSE_TEST_EQ(test, name.substr(1),
                    kNames[static_cast<std::size_t>(route)]);
  char scalar = sizeof(asc::DenseBlasRealType<T>) == 4 ? 's' : 'd';
  if constexpr (asc::DenseBlasComplex<T>) {
    scalar = sizeof(asc::DenseBlasRealType<T>) == 4 ? 'c' : 'z';
  }
  ASC_DENSE_TEST_EQ(test, name.front(), scalar);
}

template <typename T>
void Positive(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Route route, bool row_major) {
  Data<T> data(row_major);
  const auto factor_report = Prepare(test, provider, route, data);
  asc::LapackReport report;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, route, data, factor_report, report);
  }));
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, route, data, factor_report, plan,
                   data.storage.Workspace(), report);
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info &&
                                 *report.native_info == 0);
  CheckReportIdentity<T>(test, provider, route, report);
  CheckLuNumerics(test, route, data);
  CheckNumerics(test, route, data);
}

template <typename T>
void Rejected(TestContext& test, const asc::ReferenceLapackProvider& provider,
              Route route, bool row_major, std::size_t role,
              asc::MemorySpace placement) {
  // These are real, fully sized host objects. The changed tag tests admission;
  // even the old pinned-accepting negative control never receives an unsafe
  // pointer, undersized work or false factor provenance.
  Data<T> data(row_major);
  const auto factor_report = Prepare(test, provider, route, data);
  asc::LapackReport report;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, route, data, factor_report, report);
  }));
  auto workspace = data.storage.Workspace();
  const auto region = workspace.regions[role];
  workspace.regions[role] = {region.data(), region.size(), placement};
  const auto before = data.Snapshot();
  report.called_provider = true;
  report.native_info = 117;
  const auto status = WithoutAllocation(test, [&] {
    return Execute(provider, route, data, factor_report, plan, workspace,
                   report);
  });
  if (status.code() != asc::ErrorCode::kMemoryAccess) {
    std::fprintf(stderr,
                 "Admission failure route=%s row_major=%d role=%zu space=%d\n",
                 kNames[static_cast<std::size_t>(route)].data(), row_major,
                 role, static_cast<int>(placement));
  }
  ASC_DENSE_TEST_EQ(test, status.code(), asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, data.Snapshot(), before);
  ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info &&
                                 !report.native_argument &&
                                 !report.diagnostic_index);
  ASC_DENSE_TEST_EQ(test, report.output_validity,
                    asc::LapackOutputValidity::kUnchanged);
  CheckReportIdentity<T>(test, provider, route, report);
}

template <typename T>
void NeutralPolicy(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  Data<T> data(false);
  asc::LapackReport report;
  const auto plan = Take(Query(provider, Route::kGeequ, data, report, report));
  auto workspace = data.storage.Workspace();
  for (auto& region : workspace.regions) {
    region = {region.data(), region.size(), asc::MemorySpace::kPinnedHost};
  }
  const auto before = data.Snapshot();
  const auto status = WithoutAllocation(test, [&] {
    return asc::ValidateLapackWorkspace(plan, plan.identity, workspace, {});
  });
  ASC_DENSE_TEST_CHECK(test, status.ok());
  ASC_DENSE_TEST_EQ(test, data.Snapshot(), before);
  // Nonempty regions are the contextual correction. An unused zero-byte
  // pinned descriptor retains the pre-existing neutral policy.
  workspace = data.storage.Workspace();
  workspace
      .regions[static_cast<std::size_t>(asc::LapackWorkspaceKind::kScratch)] = {
      nullptr, 0, asc::MemorySpace::kPinnedHost};
  ASC_DENSE_TEST_CHECK(test, WithoutAllocation(test, [&] {
                               return Execute(provider, Route::kGeequ, data,
                                              report, plan, workspace, report);
                             }).ok());
}

template <typename T>
void Run(TestContext& test, const asc::ReferenceLapackProvider& provider) {
  NeutralPolicy<T>(test, provider);
  asc_placement_test::PivotOperands<T>(test, provider);
  for (std::size_t operation = 0; operation < kNames.size(); ++operation) {
    const auto route = static_cast<Route>(operation);
    for (bool row_major : {false, true}) {
      Positive<T>(test, provider, route, row_major);
      for (std::size_t role = 0; role < asc::LapackWorkspace{}.regions.size();
           ++role) {
        for (auto placement :
             {asc::MemorySpace::kPinnedHost, asc::MemorySpace::kDevice,
              asc::MemorySpace::kManaged}) {
          Rejected<T>(test, provider, route, row_major, role, placement);
        }
      }
    }
  }
}
}  // namespace

int main(int argc, char** argv) {
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
  const auto result = test.Finish();
  if (result == 0) {
    std::printf(
        "Placement %s: 40 host controls, 960 placement rejections passed\n",
        argv[1]);
  }
  return result;
}
