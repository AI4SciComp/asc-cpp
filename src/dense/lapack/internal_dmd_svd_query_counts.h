#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_SVD_QUERY_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_SVD_QUERY_COUNTS_H_

#include <algorithm>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_rank_revealing_counts.h"
#include "internal_svd_least_squares_counts.h"

namespace asc::internal_dmd_counts {

// The two GESVD job pairs used by GEDMD and its selected GESVDQ subproblem.
// These are query-expression bounds only, not downstream execution admission.
struct SvdQueryCounts {
  extent_t raw_preferred;
  extent_t returned_preferred;
};

template <typename Real>
Result<SvdQueryCounts> SvdQuery(extent_t m, extent_t n, bool complex,
                                bool overwrite_input, extent_t limit) {
  if (n < 1 || m < n || (!overwrite_input && m != n) ||
      !internal_lapack_rank_revealing::SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  namespace svd = internal_lapack_svd_least_squares;
  svd::Arithmetic a(limit);
  const auto crossover = a.Crossover(n);
  const auto offset = a.Product(complex ? 2 : 3, n);
  const auto qr = a.Round<Real>(a.Product(n, 32));
  const auto form_q = a.Round<Real>(a.Product(n, 32));
  // GESVD also queries the m-by-m ORG/UNGQR before its path selection, even
  // when this particular job pair never consumes that returned workspace.
  static_cast<void>(a.Round<Real>(a.Product(m, 32)));
  const auto bidiagonal = a.Round<Real>(a.Product(n, 64));
  const auto form_p = svd::FormPQuery<Real>(n, false, a);
  const auto bds = complex ? extent_t{1} : a.Product(n, 5);
  extent_t maximum = 1;
  if (m >= crossover) {
    const auto block = std::max(
        {a.Sum({n, qr}), a.Sum({n, form_q}), a.Sum({offset, bidiagonal}),
         a.Sum({offset, form_q}), a.Sum({offset, form_p}), bds});
    const auto square = a.Product(n, n);
    maximum = overwrite_input
                  ? std::max(a.Sum({square, block}),
                             a.Sum({square, a.Product(m, n), complex ? 0 : n}))
                  : a.Sum({a.Product(2, square), block});
  } else {
    const auto rectangular = a.Round<Real>(a.Product(a.Sum({m, n}), 32));
    maximum = std::max({a.Sum({offset, rectangular}), a.Sum({offset, form_q}),
                        a.Sum({offset, form_p}), bds});
  }
  const auto minimum = std::max(a.Sum({offset, m}), bds);
  maximum = std::max(maximum, minimum);
  const auto returned = a.Round<Real>(maximum);
  return a.valid() ? Result<SvdQueryCounts>(SvdQueryCounts{maximum, returned})
                   : Result<SvdQueryCounts>(Status(ErrorCode::kOverflow));
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_SVD_QUERY_COUNTS_H_
