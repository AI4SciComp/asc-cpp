#ifndef ASC_TESTS_DENSE_LAPACK_PROVIDER_PLACEMENT_ROUTES_H_
#define ASC_TESTS_DENSE_LAPACK_PROVIDER_PLACEMENT_ROUTES_H_

#include <utility>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky.h"
#include "asc/dense/providers/lapack_lu.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"
#include "asc/dense/providers/lapack_lu_refinement.h"
#include "provider_placement_support.h"

namespace asc_placement_test {
template <typename T>
asc::Result<asc::LapackWorkspacePlan> QueryLu(
    const asc::ReferenceLapackProvider& provider, Route route, Data<T>& data,
    const asc::LapackReport& factor_report, asc::LapackReport& query_report) {
  auto& v = data.values;
  const auto a = v.Matrix(v.a, 0);
  const auto af = v.Matrix(v.af, 1);
  const auto b = v.Matrix(v.b, 2);
  const auto pivots = Vector(v.pivots, 2);
  switch (route) {
    case Route::kGetrf:
      return asc::QueryGetrfWorkspace(provider, a, pivots);
    case Route::kGetrs:
      return asc::QueryGetrsWorkspace(provider, kNone,
                                      LuFactor(data, factor_report), b);
    case Route::kGetrf2:
      return asc::QueryGetrf2Workspace(provider, a, pivots);
    case Route::kGetf2:
      return asc::QueryGetf2Workspace(provider, a, pivots);
    case Route::kGetriQuery:
    case Route::kGetri:
      return asc::QueryGetriWorkspace(provider, af, v.Pivots(),
                                      data.storage.Workspace(), query_report);
    case Route::kGesv:
      return asc::QueryGesvWorkspace(provider, a, pivots, b);
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Result<asc::LapackWorkspacePlan> QueryAdvanced(
    const asc::ReferenceLapackProvider& provider, Route route, Data<T>& data) {
  using Real = asc::DenseBlasRealType<T>;
  auto& v = data.values;
  const auto a = v.Matrix(std::as_const(v.a), 0);
  const auto af = v.Matrix(std::as_const(v.af), 1);
  switch (route) {
    case Route::kGeequ:
      return asc::QueryGeequWorkspace(provider, a, Vector(v.rows, 2),
                                      Vector(v.columns, 2), data.equilibration);
    case Route::kGeequb:
      return asc::QueryGeequbWorkspace(provider, a, Vector(v.rows, 2),
                                       Vector(v.columns, 2),
                                       data.equilibration);
    case Route::kGecon:
      return asc::QueryGeconWorkspace(provider, asc::LapackConditionNorm::kOne,
                                      af, Real{4}, data.condition);
    case Route::kGerfs:
      return asc::QueryGerfsWorkspace(
          provider, kNone, a, af, v.Pivots(), v.Matrix(std::as_const(v.b), 2),
          v.Matrix(v.x, 3), Vector(v.ferr, 2), Vector(v.berr, 2));
    case Route::kGesvx:
      return v.Query(provider, kNone, 'N');
    case Route::kGesvxEquilibrated:
      return v.Query(provider, kNone, 'E');
    case Route::kGesvxFactored:
      return v.Query(provider, kNone, 'F');
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Result<asc::LapackWorkspacePlan> QueryCholesky(
    const asc::ReferenceLapackProvider& provider, Route route, Data<T>& data,
    const asc::LapackReport& factor_report) {
  auto& v = data.values;
  const auto a = v.Matrix(v.a, 0);
  switch (route) {
    case Route::kPotrf:
      return asc::QueryPotrfWorkspace(provider, kLower, a);
    case Route::kPotrf2:
      return asc::QueryPotrf2Workspace(provider, kLower, a);
    case Route::kPotf2:
      return asc::QueryPotf2Workspace(provider, kLower, a);
    case Route::kPotrs:
      return asc::QueryPotrsWorkspace(
          provider, CholeskyFactor(data, factor_report), v.Matrix(v.b, 2));
    case Route::kPotri:
      return asc::QueryPotriWorkspace(provider, kLower, v.Matrix(v.af, 1));
    case Route::kPosv:
      return asc::QueryPosvWorkspace(provider, kLower, a, v.Matrix(v.b, 2));
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Result<asc::LapackWorkspacePlan> Query(
    const asc::ReferenceLapackProvider& provider, Route route, Data<T>& data,
    const asc::LapackReport& factor_report, asc::LapackReport& query_report) {
  if (route <= Route::kGesv) {
    return QueryLu(provider, route, data, factor_report, query_report);
  }
  return route <= Route::kGesvxFactored
             ? QueryAdvanced(provider, route, data)
             : QueryCholesky(provider, route, data, factor_report);
}

template <typename T>
asc::Status ExecuteLu(const asc::ReferenceLapackProvider& provider, Route route,
                      Data<T>& data, const asc::LapackReport& factor_report,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  auto& v = data.values;
  const auto a = v.Matrix(v.a, 0);
  const auto pivots = Vector(v.pivots, 2);
  switch (route) {
    case Route::kGetrf:
      return asc::Getrf(provider, a, pivots, plan, workspace, report);
    case Route::kGetrs:
      return asc::Getrs(provider, kNone, LuFactor(data, factor_report),
                        v.Matrix(v.b, 2), plan, workspace, report);
    case Route::kGetrf2:
      return asc::Getrf2(provider, a, pivots, plan, workspace, report);
    case Route::kGetf2:
      return asc::Getf2(provider, a, pivots, plan, workspace, report);
    case Route::kGetriQuery: {
      const auto query = asc::QueryGetriWorkspace(
          provider, v.Matrix(v.af, 1), v.Pivots(), workspace, report);
      return query.status();
    }
    case Route::kGetri:
      return asc::Getri(provider, v.Matrix(v.af, 1), v.Pivots(), plan,
                        workspace, report);
    case Route::kGesv:
      return asc::Gesv(provider, a, pivots, v.Matrix(v.b, 2), plan, workspace,
                       report);
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Status ExecuteAdvanced(const asc::ReferenceLapackProvider& provider,
                            Route route, Data<T>& data,
                            const asc::LapackWorkspacePlan& plan,
                            const asc::LapackWorkspace& workspace,
                            asc::LapackReport& report) {
  using Real = asc::DenseBlasRealType<T>;
  auto& v = data.values;
  const auto a = v.Matrix(std::as_const(v.a), 0);
  const auto af = v.Matrix(std::as_const(v.af), 1);
  switch (route) {
    case Route::kGeequ:
      return asc::Geequ(provider, a, Vector(v.rows, 2), Vector(v.columns, 2),
                        data.equilibration, plan, workspace, report);
    case Route::kGeequb:
      return asc::Geequb(provider, a, Vector(v.rows, 2), Vector(v.columns, 2),
                         data.equilibration, plan, workspace, report);
    case Route::kGecon:
      return asc::Gecon(provider, asc::LapackConditionNorm::kOne, af, Real{4},
                        data.condition, plan, workspace, report);
    case Route::kGerfs:
      return asc::Gerfs(provider, kNone, a, af, v.Pivots(),
                        v.Matrix(std::as_const(v.b), 2), v.Matrix(v.x, 3),
                        Vector(v.ferr, 2), Vector(v.berr, 2), plan, workspace,
                        report);
    case Route::kGesvx:
      return v.Execute(provider, kNone, 'N', plan, workspace, report);
    case Route::kGesvxEquilibrated:
      return v.Execute(provider, kNone, 'E', plan, workspace, report);
    case Route::kGesvxFactored:
      return v.Execute(provider, kNone, 'F', plan, workspace, report);
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Status ExecuteCholesky(const asc::ReferenceLapackProvider& provider,
                            Route route, Data<T>& data,
                            const asc::LapackReport& factor_report,
                            const asc::LapackWorkspacePlan& plan,
                            const asc::LapackWorkspace& workspace,
                            asc::LapackReport& report) {
  auto& v = data.values;
  const auto a = v.Matrix(v.a, 0);
  switch (route) {
    case Route::kPotrf:
      return asc::Potrf(provider, kLower, a, plan, workspace, report);
    case Route::kPotrf2:
      return asc::Potrf2(provider, kLower, a, plan, workspace, report);
    case Route::kPotf2:
      return asc::Potf2(provider, kLower, a, plan, workspace, report);
    case Route::kPotrs:
      return asc::Potrs(provider, CholeskyFactor(data, factor_report),
                        v.Matrix(v.b, 2), plan, workspace, report);
    case Route::kPotri:
      return asc::Potri(provider, kLower, v.Matrix(v.af, 1), plan, workspace,
                        report);
    case Route::kPosv:
      return asc::Posv(provider, kLower, a, v.Matrix(v.b, 2), plan, workspace,
                       report);
    default:
      return asc::Status(asc::ErrorCode::kInvalidArgument);
  }
}

template <typename T>
asc::Status Execute(const asc::ReferenceLapackProvider& provider, Route route,
                    Data<T>& data, const asc::LapackReport& factor_report,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if (route <= Route::kGesv) {
    return ExecuteLu(provider, route, data, factor_report, plan, workspace,
                     report);
  }
  return route <= Route::kGesvxFactored
             ? ExecuteAdvanced(provider, route, data, plan, workspace, report)
             : ExecuteCholesky(provider, route, data, factor_report, plan,
                               workspace, report);
}
}  // namespace asc_placement_test

#endif  // ASC_TESTS_DENSE_LAPACK_PROVIDER_PLACEMENT_ROUTES_H_
