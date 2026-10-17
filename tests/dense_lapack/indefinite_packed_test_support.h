#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_TEST_SUPPORT_H_
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_packed.h"
#include "indefinite_packed_fixture.h"
#include "indefinite_test_support.h"
namespace asc_packed_indefinite_test {
namespace base = asc_indefinite_test;
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle tri, bool he,
           asc::DenseBlasPackedMatrixView<T> a,
           asc::DenseBlasVectorView<asc::index_t> p) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHptrfWorkspace(provider, tri, a, p);
    }
  }
  return asc::QuerySptrfWorkspace(provider, tri, a, p);
}
template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle tri, bool he,
                   asc::DenseBlasPackedMatrixView<T> a,
                   asc::DenseBlasVectorView<asc::index_t> p,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::Hptrf(provider, tri, a, p, plan, workspace, report);
    }
  }
  return asc::Sptrf(provider, tri, a, p, plan, workspace, report);
}
}  // namespace asc_packed_indefinite_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_PACKED_TEST_SUPPORT_H_
