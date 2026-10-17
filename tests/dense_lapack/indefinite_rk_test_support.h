#ifndef ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_TEST_SUPPORT_H_
#include <array>
#include <cstddef>

#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_indefinite_rk.h"
#include "indefinite_rook_test_support.h"
namespace asc_rk_test {
using asc_indefinite_rook_test::Adjoint;
using asc_indefinite_rook_test::EqualBytes;
using asc_indefinite_rook_test::kColumn;
using asc_indefinite_rook_test::kHost;
using asc_indefinite_rook_test::kLower;
using asc_indefinite_rook_test::kRow;
using asc_indefinite_rook_test::kUpper;
using asc_indefinite_rook_test::Matrix;
using asc_indefinite_rook_test::Pivots;
using asc_indefinite_rook_test::Scratch;
using asc_indefinite_rook_test::Take;
using asc_indefinite_rook_test::TestContext;
using asc_indefinite_rook_test::ToWide;
using asc_indefinite_rook_test::Value;
using asc_indefinite_rook_test::Wide;
using asc_indefinite_rook_test::WithoutAllocation;
template <typename T, std::size_t Size>
auto OffDiagonal(std::array<T, Size>& data, asc::extent_t n) {
  return Take(asc::DenseBlasVectorView<T>::Create(
      data.data() + 1, n, 1, {data.data(), sizeof(data), kHost}));
}
template <typename T>
auto QueryFactor(const asc::ReferenceLapackProvider& provider,
                 asc::DenseBlasTriangle triangle, bool hermitian, bool blocked,
                 asc::DenseBlasMatrixView<T> matrix,
                 asc::DenseBlasVectorView<T> off_diagonal,
                 asc::DenseBlasVectorView<asc::index_t> pivots) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::QueryHetrfRkWorkspace(provider, triangle, matrix,
                                                  off_diagonal, pivots)
                     : asc::QueryHetf2RkWorkspace(provider, triangle, matrix,
                                                  off_diagonal, pivots);
    }
  }
  return blocked ? asc::QuerySytrfRkWorkspace(provider, triangle, matrix,
                                              off_diagonal, pivots)
                 : asc::QuerySytf2RkWorkspace(provider, triangle, matrix,
                                              off_diagonal, pivots);
}

template <typename T>
asc::Status Factor(const asc::ReferenceLapackProvider& provider,
                   asc::DenseBlasTriangle triangle, bool hermitian,
                   bool blocked, asc::DenseBlasMatrixView<T> matrix,
                   asc::DenseBlasVectorView<T> off_diagonal,
                   asc::DenseBlasVectorView<asc::index_t> pivots,
                   const asc::LapackWorkspacePlan& plan,
                   const asc::LapackWorkspace& workspace,
                   asc::LapackReport& report) {
  if constexpr (asc::DenseBlasComplex<T>) {
    if (hermitian) {
      return blocked ? asc::HetrfRk(provider, triangle, matrix, off_diagonal,
                                    pivots, plan, workspace, report)
                     : asc::Hetf2Rk(provider, triangle, matrix, off_diagonal,
                                    pivots, plan, workspace, report);
    }
  }
  return blocked ? asc::SytrfRk(provider, triangle, matrix, off_diagonal,
                                pivots, plan, workspace, report)
                 : asc::Sytf2Rk(provider, triangle, matrix, off_diagonal,
                                pivots, plan, workspace, report);
}

}  // namespace asc_rk_test
#endif  // ASC_TESTS_DENSE_LAPACK_INDEFINITE_RK_TEST_SUPPORT_H_
