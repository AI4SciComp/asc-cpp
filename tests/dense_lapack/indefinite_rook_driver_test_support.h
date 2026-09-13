#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_TEST_SUPPORT_H_
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rook_driver.h"
namespace asc_rook_driver_test {
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasVectorView<asc::index_t> pivots,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHesvRookWorkspace(provider, triangle, a, pivots, b);
    }
  }
  return asc::QuerySysvRookWorkspace(provider, triangle, a, pivots, b);
}

template <typename T>
asc::Status Driver(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   asc::DenseBlasMatrixView<T> a,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   asc::DenseBlasMatrixView<T> b,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::HesvRook(provider, triangle, a, pivots, b, plan, workspace,
                           report);
    }
  }
  return asc::SysvRook(provider, triangle, a, pivots, b, plan, workspace,
                       report);
}

}  // namespace asc_rook_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_ROOK_DRIVER_TEST_SUPPORT_H_
