#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_TEST_SUPPORT_H_

#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook_condition.h"

namespace asc_rook_condition_test {
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> a, asc::RawLapackPivotView pivots,
           asc::DenseBlasRealType<T> norm,
           const asc::DenseBlasRealType<T>& condition) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHeconRookWorkspace(provider, triangle, a, pivots, norm,
                                          condition);
    }
  }
  return asc::QuerySyconRookWorkspace(provider, triangle, a, pivots, norm,
                                      condition);
}

template <typename T>
asc::Status Condition(const asc::ReferenceLapackProvider& provider,
                      asc::DenseBlasTriangle triangle, bool hermitian,
                      asc::DenseBlasMatrixView<const T> a,
                      asc::RawLapackPivotView pivots,
                      asc::DenseBlasRealType<T> norm,
                      asc::DenseBlasRealType<T>& condition,
                      const asc::LapackWorkspacePlan& plan,
                      const asc::LapackWorkspace& workspace,
                      asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HeconRook(provider, triangle, a, pivots, norm, condition,
                            plan, workspace, report);
    }
  }
  return asc::SyconRook(provider, triangle, a, pivots, norm, condition, plan,
                        workspace, report);
}

}  // namespace asc_rook_condition_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_CONDITION_TEST_SUPPORT_H_
