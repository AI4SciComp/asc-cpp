#ifndef ASC_TESTS_DENSE_LAPACK_PROVIDER_OPERAND_PLACEMENT_TEST_H_
#define ASC_TESTS_DENSE_LAPACK_PROVIDER_OPERAND_PLACEMENT_TEST_H_

#include <utility>

#include "../dense/test_support.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_lu.h"
#include "provider_placement_routes.h"
#include "provider_placement_support.h"

namespace asc_placement_test {
template <typename T>
auto TaggedPivotOutput(Data<T>& data, asc::MemorySpace space) {
  auto& values = data.values.pivots;
  return Take(asc::DenseBlasVectorView<asc::index_t>::Create(
      values.data() + 1, 2, 1, {values.data(), sizeof(values), space}));
}

template <typename T>
auto TaggedRawPivots(const Data<T>& data, asc::MemorySpace space) {
  const auto& values = data.values.pivots;
  return Take(asc::RawLapackPivotView::Create(
      values.data() + 1, 2, asc::LapackFactorFamily::kLuPartialPivot,
      {values.data(), sizeof(values), space}));
}

template <typename T>
auto TaggedLuFactor(const Data<T>& data, const asc::LapackReport& factor_report,
                    asc::MemorySpace space) {
  return Take(asc::LapackLuFactorView<T>::Create(
      data.values.Matrix(data.values.af, 1), TaggedRawPivots(data, space),
      factor_report));
}

template <typename T>
asc::Result<asc::LapackWorkspacePlan> QueryPivotOperand(
    const asc::ReferenceLapackProvider& provider, Route route, Data<T>& data,
    const asc::LapackReport& factor_report, asc::MemorySpace space,
    asc::LapackReport& report) {
  auto& v = data.values;
  const auto pivots = TaggedPivotOutput(data, space);
  switch (route) {
    case Route::kGetrf:
      return asc::QueryGetrfWorkspace(provider, v.Matrix(v.a, 0), pivots);
    case Route::kGetrf2:
      return asc::QueryGetrf2Workspace(provider, v.Matrix(v.a, 0), pivots);
    case Route::kGetf2:
      return asc::QueryGetf2Workspace(provider, v.Matrix(v.a, 0), pivots);
    case Route::kGesv:
      return asc::QueryGesvWorkspace(provider, v.Matrix(v.a, 0), pivots,
                                     v.Matrix(v.b, 2));
    case Route::kGetrs:
      return asc::QueryGetrsWorkspace(
          provider, kNone, TaggedLuFactor(data, factor_report, space),
          v.Matrix(v.b, 2));
    case Route::kGetriQuery:
    case Route::kGetri:
      return asc::QueryGetriWorkspace(provider, v.Matrix(v.af, 1),
                                      TaggedRawPivots(data, space),
                                      data.storage.Workspace(), report);
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Status ExecutePivotOperand(const asc::ReferenceLapackProvider& provider,
                                Route route, Data<T>& data,
                                const asc::LapackReport& factor_report,
                                asc::MemorySpace space,
                                const asc::LapackWorkspacePlan& plan,
                                asc::LapackReport& report) {
  auto& v = data.values;
  const auto pivots = TaggedPivotOutput(data, space);
  const auto workspace = data.storage.Workspace();
  switch (route) {
    case Route::kGetrf:
      return asc::Getrf(provider, v.Matrix(v.a, 0), pivots, plan, workspace,
                        report);
    case Route::kGetrf2:
      return asc::Getrf2(provider, v.Matrix(v.a, 0), pivots, plan, workspace,
                         report);
    case Route::kGetf2:
      return asc::Getf2(provider, v.Matrix(v.a, 0), pivots, plan, workspace,
                        report);
    case Route::kGesv:
      return asc::Gesv(provider, v.Matrix(v.a, 0), pivots, v.Matrix(v.b, 2),
                       plan, workspace, report);
    case Route::kGetrs:
      return asc::Getrs(provider, kNone,
                        TaggedLuFactor(data, factor_report, space),
                        v.Matrix(v.b, 2), plan, workspace, report);
    case Route::kGetriQuery:
      return QueryPivotOperand(provider, route, data, factor_report, space,
                               report)
          .status();
    case Route::kGetri:
      return asc::Getri(provider, v.Matrix(v.af, 1),
                        TaggedRawPivots(data, space), plan, workspace, report);
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
void PivotOperandCase(TestContext& test,
                      const asc::ReferenceLapackProvider& provider, Route route,
                      bool row_major, asc::MemorySpace space) {
  Data<T> data(row_major);
  const auto factor_report = Prepare(test, provider, route, data);
  asc::LapackReport report;
  const auto plan = Take(WithoutAllocation(test, [&] {
    return Query(provider, route, data, factor_report, report);
  }));
  const auto before = data.Snapshot();
  const auto query = WithoutAllocation(test, [&] {
    return QueryPivotOperand(provider, route, data, factor_report, space,
                             report);
  });
  const bool host = space == kHost;
  ASC_DENSE_TEST_EQ(test, query.status().code(),
                    host ? asc::ErrorCode::kOk : asc::ErrorCode::kMemoryAccess);
  ASC_DENSE_TEST_EQ(test, data.Snapshot(), before);
  if (!host && (route == Route::kGetri || route == Route::kGetriQuery)) {
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info);
  }
  report.called_provider = true;
  report.native_info = 119;
  const auto status = WithoutAllocation(test, [&] {
    return ExecutePivotOperand(provider, route, data, factor_report, space,
                               plan, report);
  });
  ASC_DENSE_TEST_EQ(test, status.code(),
                    host ? asc::ErrorCode::kOk : asc::ErrorCode::kMemoryAccess);
  if (host) {
    ASC_DENSE_TEST_CHECK(test, report.called_provider && report.native_info &&
                                   *report.native_info == 0);
  } else {
    ASC_DENSE_TEST_EQ(test, data.Snapshot(), before);
    ASC_DENSE_TEST_CHECK(test, !report.called_provider && !report.native_info &&
                                   !report.native_argument &&
                                   !report.diagnostic_index);
    ASC_DENSE_TEST_EQ(test, report.output_validity,
                      asc::LapackOutputValidity::kUnchanged);
  }
}

template <typename T>
void PivotOperands(TestContext& test,
                   const asc::ReferenceLapackProvider& provider) {
  for (auto route :
       {Route::kGetrf, Route::kGetrs, Route::kGetrf2, Route::kGetf2,
        Route::kGetriQuery, Route::kGetri, Route::kGesv}) {
    for (bool row_major : {false, true}) {
      for (auto space : {kHost, asc::MemorySpace::kPinnedHost}) {
        PivotOperandCase<T>(test, provider, route, row_major, space);
      }
    }
  }
}
}  // namespace asc_placement_test

#endif  // ASC_TESTS_DENSE_LAPACK_PROVIDER_OPERAND_PLACEMENT_TEST_H_
