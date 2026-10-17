#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_TEST_SUPPORT_H_
#include <array>
#include <cstddef>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/providers/lapack_indefinite_aasen_solve.h"
#include "indefinite_rook_test_support.h"
namespace asc_aasen_solve_test {
template <std::size_t Size>
auto Raw(const std::array<asc::index_t, Size>& values, asc::extent_t n) {
  return asc_indefinite_rook_test::Take(asc::RawLapackPivotView::Create(
      values.data() + 1, n, asc::LapackFactorFamily::kAasen,
      {values.data(), sizeof(values), asc_indefinite_rook_test::kHost}));
}
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider, bool he,
           asc::DenseBlasTriangle triangle,
           asc::DenseBlasMatrixView<const T> factors,
           asc::RawLapackPivotView pivots, asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::QueryHetrsAaWorkspace(provider, triangle, factors, pivots,
                                        rhs);
    }
  }
  return asc::QuerySytrsAaWorkspace(provider, triangle, factors, pivots, rhs);
}
template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider, bool he,
                  asc::DenseBlasTriangle triangle,
                  asc::DenseBlasMatrixView<const T> factors,
                  asc::RawLapackPivotView pivots,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (he) {
      return asc::HetrsAa(provider, triangle, factors, pivots, rhs, plan,
                          workspace, report);
    }
  }
  return asc::SytrsAa(provider, triangle, factors, pivots, rhs, plan, workspace,
                      report);
}
}  // namespace asc_aasen_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_AASEN_SOLVE_TEST_SUPPORT_H_
