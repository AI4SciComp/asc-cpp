#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_PRECONDITIONED_QUERY_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_PRECONDITIONED_QUERY_COUNTS_H_

#include <algorithm>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_dmd_svd_query_counts.h"
#include "internal_rank_revealing_counts.h"
#include "internal_svd_least_squares_counts.h"

namespace asc::internal_dmd_counts {

// Only the H,P,N,R,R GESVDQ and complex F,U,J,N/R,N,P GEJSV queries
// selected by GEDMD. Execution has additional, data-dependent rank paths.
template <typename Real>
Result<SvdQueryCounts> PreconditionedQuery(extent_t m, extent_t n, bool complex,
                                           bool jacobi, extent_t limit) {
  if (n < 1 || m < n || (jacobi && !complex) ||
      !internal_lapack_rank_revealing::SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  internal_lapack_svd_least_squares::Arithmetic a(limit);
  const auto qr = a.Round<Real>(a.Product(n, 32));
  // GEQP3's outer S query rounds upward; C/D/Z encode a plain scalar.
  const auto pivoted =
      a.Round<Real>(a.Sum({a.Product(n, complex ? 32 : 34), 32}),
                    std::is_same_v<Real, float> && !complex);
  const auto apply = a.Round<Real>(a.Sum({a.Product(n, 32), 4160}));
  extent_t maximum = 2;
  if (jacobi) {
    // Both GEQRF and the unused GELQF query return n*32. The Jacobi
    // subproblem has n rows/columns and returns 2*n scalar entries.
    const auto jacobi_work = a.Round<Real>(a.Product(n, 2));
    const auto base = a.Sum({a.Product(n, n), a.Product(n, 2)});
    const auto minimum =
        std::max(a.Sum({a.Product(n, n), a.Product(n, 4)}), a.Sum({n, m}));
    maximum = std::max({a.Sum({n, pivoted}), a.Sum({a.Product(n, 2), qr}), base,
                        a.Sum({base, jacobi_work}), a.Sum({base, n, apply}),
                        a.Sum({n, apply}), minimum});
  } else {
    const auto svd = SvdQuery<Real>(n, n, complex, false, limit);
    if (!svd.ok()) {
      return svd.status();
    }
    maximum = a.Sum({n, std::max({pivoted, svd->returned_preferred, apply})});
  }
  // Both enclosing routines use a plain scalar cast, including binary32.
  const auto returned = a.Round<Real>(maximum, false);
  return a.valid() ? Result<SvdQueryCounts>(SvdQueryCounts{maximum, returned})
                   : Result<SvdQueryCounts>(Status(ErrorCode::kOverflow));
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_PRECONDITIONED_QUERY_COUNTS_H_
