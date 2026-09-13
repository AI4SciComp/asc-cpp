#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_TEST_SUPPORT_H_
#include <array>
#include <cstddef>

#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk_solve.h"
#include "indefinite_rk_test_support.h"
namespace asc_rk_solve_test {
template <typename T, std::size_t Size>
auto OffDiagonal(const std::array<T, Size>& data, asc::extent_t n) {
  return asc_rk_test::Take(asc::DenseBlasVectorView<const T>::Create(
      data.data() + 1, n, 1,
      {data.data(), sizeof(data), asc::MemorySpace::kHost}));
}
template <typename T>
auto Query(const asc::ReferenceLapackProvider& provider,
           asc::DenseBlasTriangle triangle, bool hermitian,
           asc::DenseBlasMatrixView<const T> factors,
           asc::DenseBlasVectorView<const T> off_diagonal,
           asc::RawLapackPivotView pivots, asc::DenseBlasMatrixView<T> rhs) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::QueryHetrs3Workspace(provider, triangle, factors,
                                       off_diagonal, pivots, rhs);
    }
  }
  return asc::QuerySytrs3Workspace(provider, triangle, factors, off_diagonal,
                                   pivots, rhs);
}
template <typename T>
asc::Status Solve(const asc::ReferenceLapackProvider& provider,
                  asc::DenseBlasTriangle triangle, bool hermitian,
                  asc::DenseBlasMatrixView<const T> factors,
                  asc::DenseBlasVectorView<const T> off_diagonal,
                  asc::RawLapackPivotView pivots,
                  asc::DenseBlasMatrixView<T> rhs,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return asc::Hetrs3(provider, triangle, factors, off_diagonal, pivots, rhs,
                         plan, workspace, report);
    }
  }
  return asc::Sytrs3(provider, triangle, factors, off_diagonal, pivots, rhs,
                     plan, workspace, report);
}
}  // namespace asc_rk_solve_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_SOLVE_TEST_SUPPORT_H_
