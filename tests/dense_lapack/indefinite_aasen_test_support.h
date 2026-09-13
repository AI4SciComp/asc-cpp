#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TEST_SUPPORT_H_
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_aasen.h"
namespace asc_aasen_test {
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool he,
           asc::DenseBlasMatrixView<T> matrix,
           asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrfAaWorkspace(provider, triangle, matrix, pivots);
    }
  }
  return asc::QuerySytrfAaWorkspace(provider, triangle, matrix, pivots);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool he,
                   asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrfAa(provider, triangle, matrix, pivots, plan, workspace,
                          report);
    }
  }
  return asc::SytrfAa(provider, triangle, matrix, pivots, plan, workspace,
                      report);
}
}  // namespace asc_aasen_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TEST_SUPPORT_H_
