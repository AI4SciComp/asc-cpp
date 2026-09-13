#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INVERSE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INVERSE_TEST_SUPPORT_H_
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook_inverse.h"
namespace asc_rook_inverse_test {
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> factors,
           asc::RawLapackPivotView pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetriRookWorkspace(provider, triangle, factors, pivots);
    }
  }
  return asc::QuerySytriRookWorkspace(provider, triangle, factors, pivots);
}
template <typename T>
asc::Status Inverse(const asc::ReferenceLapackProvider& provider,
                    asc::DenseBlasTriangle triangle, bool hermitian,
                    asc::DenseBlasMatrixView<T> factors,
                    asc::RawLapackPivotView pivots,
                    const asc::LapackWorkspacePlan& plan,
                    const asc::LapackWorkspace& workspace,
                    asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HetriRook(provider, triangle, factors, pivots, plan,
                            workspace, report);
    }
  }
  return asc::SytriRook(provider, triangle, factors, pivots, plan, workspace,
                        report);
}
}  // namespace asc_rook_inverse_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_INVERSE_TEST_SUPPORT_H_
