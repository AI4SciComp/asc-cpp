#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_TEST_SUPPORT_H_
#include <vector>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage_driver.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_test_support.h"
namespace asc_aasen_two_stage_driver_test {
namespace base = asc_indefinite_rook_test;
namespace factor = asc_aasen_two_stage_test;
template <typename T>
using Sample = asc_aasen_two_stage_solve_test::Sample<T>;
using factor::Vector;
template <typename T>
auto Matrix(Sample<T>& sample) {
  return base::Take(asc::DenseBlasMatrixView<T>::Create(
      sample.a.data() + 1, sample.n, sample.n,
      sample.row ? base::kRow : base::kColumn, sample.original.lda,
      {sample.a.data(), sample.a.size() * sizeof(T), base::kHost}));
}
template <typename T>
auto Rhs(Sample<T>& sample) {
  return base::Take(asc::DenseBlasMatrixView<T>::Create(
      sample.b.data() + 1, sample.n, sample.nrhs,
      sample.rhs_row ? base::kRow : base::kColumn, sample.ldb,
      {sample.b.data(), sample.b.size() * sizeof(T), base::kHost}));
}
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle tri, bool he, asc::DenseBlasMatrixView<T> a,
           asc::DenseBlasVectorView<T> tb,
           asc::DenseBlasVectorView<asc::index_t> p,
           asc::DenseBlasVectorView<asc::index_t> q,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHesvAa2StageWorkspace(provider, tri, a, tb, p, q, b);
    }
  }
  return asc::QuerySysvAa2StageWorkspace(provider, tri, a, tb, p, q, b);
}
template <typename T>
asc::Status Driver(
    const asc::ReferenceLapackProvider& provider, asc::DenseBlasTriangle tri,
    bool he, asc::DenseBlasMatrixView<T> a, asc::DenseBlasVectorView<T> tb,
    asc::DenseBlasVectorView<asc::index_t> p,
    asc::DenseBlasVectorView<asc::index_t> q, asc::DenseBlasMatrixView<T> b,
    const asc::LapackWorkspacePlan& plan, const asc::LapackWorkspace& workspace,
    asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HesvAa2Stage(provider, tri, a, tb, p, q, b, plan, workspace,
                               report);
    }
  }
  return asc::SysvAa2Stage(provider, tri, a, tb, p, q, b, plan, workspace,
                           report);
}
}  // namespace asc_aasen_two_stage_driver_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_TEST_SUPPORT_H_
