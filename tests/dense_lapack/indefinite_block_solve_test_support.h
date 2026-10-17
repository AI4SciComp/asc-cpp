#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_TEST_SUPPORT_H_
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_block_solve.h"
namespace asc_block_solve_test {
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> factors,
           asc::RawLapackPivotView pivots, asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetrs2Workspace(provider, triangle, factors, pivots,
                                       rhs);
    }
  }
  return asc::QuerySytrs2Workspace(provider, triangle, factors, pivots, rhs);
}
template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, bool hermitian,
                  asc::DenseBlasMatrixView<const T> factors,
                  asc::RawLapackPivotView pivots,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hetrs2(provider, triangle, factors, pivots, rhs, plan,
                         workspace, report);
    }
  }
  return asc::Sytrs2(provider, triangle, factors, pivots, rhs, plan, workspace,
                     report);
}
}  // namespace asc_block_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_BLOCK_SOLVE_TEST_SUPPORT_H_
