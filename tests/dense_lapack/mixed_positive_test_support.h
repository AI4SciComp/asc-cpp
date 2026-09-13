#ifndef ASC_TESTS_DENSE_LAPACK_MIXED_POSITIVE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_LAPACK_MIXED_POSITIVE_TEST_SUPPORT_H_
#include <utility>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_mixed_positive.h"
#include "mixed_general_test_support.h"  // IWYU pragma: export
namespace asc_mixed_positive_test {
using asc_mixed_test::kColumn;
using asc_mixed_test::kHost;
using asc_mixed_test::kLayout;
using asc_mixed_test::kRow;
using asc_mixed_test::kScalar;
using asc_mixed_test::Narrow;
using asc_mixed_test::Rhs;
using asc_mixed_test::Scratch;
using asc_mixed_test::Take;
using asc_mixed_test::TestContext;
using asc_mixed_test::ToWide;
using asc_mixed_test::Value;
using asc_mixed_test::Wide;
using asc_mixed_test::WithoutAllocation;
template <typename T>
struct Problem {
  Rhs<T> a;
  Rhs<T> b;
  Rhs<T> x;
  asc::DenseBlasTriangle uplo;
  Problem(asc::extent_t n, asc::extent_t nrhs, asc::DenseBlasLayout al,
          asc::DenseBlasLayout bl, asc::DenseBlasLayout xl,
          asc::DenseBlasTriangle triangle = asc::DenseBlasTriangle::kLower)
      : a(n, n, al), b(n, nrhs, bl), x(n, nrhs, xl), uplo(triangle) {}
  auto Query(const asc::ReferenceLapackProvider& provider) {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::QueryZcposvWorkspace(provider, a.View(), uplo,
                                       std::as_const(b).View(), x.View());
    } else {
      return asc::QueryDsposvWorkspace(provider, a.View(), uplo,
                                       std::as_const(b).View(), x.View());
    }
  }
  asc::Status Run(const asc::ReferenceLapackProvider& provider,
                  const asc::LapackWorkspacePlan& plan,
                  const asc::LapackWorkspace& workspace,
                  asc::LapackMixedSolveStatistics& statistics,
                  asc::LapackReport& report) {
    if constexpr (asc::DenseBlasComplex<T>) {
      return asc::Zcposv(provider, a.View(), uplo, std::as_const(b).View(),
                         x.View(), plan, workspace, statistics, report);
    } else {
      return asc::Dsposv(provider, a.View(), uplo, std::as_const(b).View(),
                         x.View(), plan, workspace, statistics, report);
    }
  }
};

}  // namespace asc_mixed_positive_test
#endif  // ASC_TESTS_DENSE_LAPACK_MIXED_POSITIVE_TEST_SUPPORT_H_
