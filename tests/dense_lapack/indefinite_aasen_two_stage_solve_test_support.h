#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_TEST_SUPPORT_H_
#include <vector>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack_indefinite_aasen_two_stage_solve.h"
#include "indefinite_aasen_two_stage_solve_fixture.h"
#include "indefinite_aasen_two_stage_test_support.h"
namespace asc_aasen_two_stage_solve_test {
template <typename T>
auto Vector(const std::vector<T>& values, asc::extent_t count) {
  return base::Take(asc::DenseBlasVectorView<const T>::Create(
      values.data() + 1, count, 1,
      {values.data(), values.size() * sizeof(T), base::kHost}));
}
template <typename T>
auto Factors(const Sample<T>& sample) {
  return base::Take(asc::DenseBlasMatrixView<const T>::Create(
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
auto Pivots(const Sample<T>& sample) {
  return base::Take(asc::RawLapackPivotView::Create(
      sample.p.data() + 1, sample.n, asc::LapackFactorFamily::kAasen,
      {sample.p.data(), sample.p.size() * sizeof(asc::index_t), base::kHost}));
}
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle tri, bool he,
           asc::DenseBlasMatrixView<const T> a,
           asc::DenseBlasVectorView<const T> tb, asc::RawLapackPivotView p,
           asc::DenseBlasVectorView<const asc::index_t> q,
           asc::DenseBlasMatrixView<T> b) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrsAa2StageWorkspace(provider, tri, a, tb, p, q, b);
    }
  }
  return asc::QuerySytrsAa2StageWorkspace(provider, tri, a, tb, p, q, b);
}
template <typename T>
asc::Status Solve(
    const asc::ReferenceLapackProvider& provider, asc::DenseBlasTriangle tri,
    bool he, asc::DenseBlasMatrixView<const T> a,
    asc::DenseBlasVectorView<const T> tb, asc::RawLapackPivotView p,
    asc::DenseBlasVectorView<const asc::index_t> q,
    asc::DenseBlasMatrixView<T> b, const asc::LapackWorkspacePlan& plan,
    const asc::LapackWorkspace& workspace, asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrsAa2Stage(provider, tri, a, tb, p, q, b, plan, workspace,
                                report);
    }
  }
  return asc::SytrsAa2Stage(provider, tri, a, tb, p, q, b, plan, workspace,
                            report);
}
}  // namespace asc_aasen_two_stage_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_TEST_SUPPORT_H_
