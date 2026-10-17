#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_DIVIDE_QUERY_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_DIVIDE_QUERY_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_dmd_svd_query_counts.h"
#include "internal_rank_revealing_counts.h"
#include "internal_svd_least_squares_counts.h"

namespace asc::internal_dmd_counts {

template <typename Real>
Result<extent_t> DivideCrossover(extent_t n, int numerator, int denominator,
                                 extent_t limit) {
  // Match the selected source precision and order before INTEGER conversion.
  const Real value = static_cast<Real>(n) * static_cast<Real>(numerator) /
                     static_cast<Real>(denominator);
  const Real bound = std::ldexp(
      Real{1}, limit == std::numeric_limits<std::int32_t>::max() ? 31 : 63);
  if (!std::isfinite(value) || value < 0 || value >= bound) {
    return Status(ErrorCode::kOverflow);
  }
  return static_cast<extent_t>(value);
}

// GESDD('O',m,n), m>=n>=1. These are the selected query expressions,
// including queries whose returned values are not used in the final branch.
template <typename Real>
Result<SvdQueryCounts> DivideQuery(extent_t m, extent_t n, bool complex,
                                   extent_t limit) {
  if (n < 1 || m < n ||
      !internal_lapack_rank_revealing::SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  namespace svd = internal_lapack_svd_least_squares;
  svd::Arithmetic a(limit);
  const auto crossover =
      DivideCrossover<Real>(n, complex ? 17 : 11, complex ? 9 : 6, limit);
  const auto secondary = DivideCrossover<Real>(n, 5, 3, limit);
  if (!crossover.ok() || !secondary.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto qr = a.Round<Real>(a.Product(n, 32));
  const auto all_q = a.Round<Real>(a.Product(m, 32));
  static_cast<void>(all_q);
  const auto bidiagonal = a.Round<Real>(a.Product(n, 64));
  const auto rectangular = a.Round<Real>(a.Product(a.Sum({m, n}), 32));
  const auto form_p = svd::FormPQuery<Real>(n, false, a);
  // ORM/UNMBR's own query is NW*32. Its later ORM/UNMQR execution
  // computes the additional fixed T storage; that is a separate guard.
  const auto apply = a.Round<Real>(a.Product(n, 32));
  const auto square = a.Product(n, n);
  const auto matrix = a.Product(m, n);
  const auto offset = a.Product(complex ? 2 : 3, n);
  const auto bds =
      complex ? extent_t{0} : a.Sum({a.Product(3, square), a.Product(4, n)});
  extent_t maximum = 1;
  extent_t minimum = 1;
  if (m >= *crossover) {
    const auto block = std::max({a.Sum({n, qr}), a.Sum({offset, bidiagonal}),
                                 a.Sum({offset, apply}), a.Sum({offset, bds})});
    maximum = a.Sum({block, square, complex ? matrix : square});
    minimum = a.Sum({a.Product(2, square), a.Product(3, n), bds});
  } else {
    const auto transformation =
        complex && m >= *secondary ? std::max(form_p, qr) : apply;
    maximum =
        a.Sum({matrix, offset, std::max({rectangular, transformation, bds})});
    minimum = complex ? a.Sum({offset, m, square})
                      : a.Sum({offset, std::max(m, a.Sum({square, bds}))});
  }
  maximum = std::max(maximum, minimum);
  // Unlike GESVD/GEEV, all four GESDD variants explicitly round upward,
  // including D/Z in an INTEGER64 build above binary64's exact integers.
  const auto returned = a.Round<Real>(maximum, true);
  return a.valid() ? Result<SvdQueryCounts>(SvdQueryCounts{maximum, returned})
                   : Result<SvdQueryCounts>(Status(ErrorCode::kOverflow));
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_DIVIDE_QUERY_COUNTS_H_
