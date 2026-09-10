#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_EIGEN_QUERY_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_EIGEN_QUERY_COUNTS_H_

#include <algorithm>
#include <cmath>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_rank_revealing_counts.h"
#include "internal_svd_least_squares_counts.h"

namespace asc::internal_dmd_counts {

struct EigenQueryCounts {
  extent_t raw_preferred;
  extent_t returned_preferred;
};

// Source-pinned IPARMQ window policy. The default-REAL logarithmic selector is
// active only on [150,589]. At larger dimensions its unused intermediate has
// denominator in [7,63] and quotient <= n, before the fixed selector wins.
inline extent_t ShiftCount(extent_t n) {
  extent_t count = 2;
  if (n >= 6000) {
    count = 256;
  } else if (n >= 3000) {
    count = 128;
  } else if (n >= 590) {
    count = 64;
  } else if (n >= 150) {
    // Preserve the pinned default-REAL LOG expression in the query oracle.
    // NOLINTNEXTLINE(modernize-use-std-numbers)
    const float logarithm = std::log(static_cast<float>(n)) / std::log(2.F);
    count = std::max<extent_t>(
        10, n / static_cast<extent_t>(std::round(logarithm)));
  } else if (n >= 60) {
    count = 10;
  } else if (n >= 30) {
    count = 4;
  }
  return count - count % 2;
}

// For the full [1,n] query, LAQR3's window is at most385. Its GEHRD/ORMHR
// queries give32*window+4160, then add window. The nested LAQR4 window is
// smaller, and the LAQR5 query term is at most384; neither raises this result.
// All window arithmetic is exact even in binary32. This models queries only.
inline extent_t HessenbergQuery(extent_t n) {
  if (n <= 15) {
    return std::max<extent_t>(1, n);
  }
  const auto shifts = ShiftCount(n);
  const auto recommended = n <= 500 ? shifts : 3 * shifts / 2;
  const auto window = std::min((n - 1) / 3, recommended) + 1;
  return std::max(n, 33 * window + 4160);
}

template <typename Real>
Result<EigenQueryCounts> EigenQuery(extent_t n, bool complex, bool vectors,
                                    extent_t limit) {
  if (n < 0 || !internal_lapack_rank_revealing::SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return EigenQueryCounts{1, 1};
  }
  internal_lapack_svd_least_squares::Arithmetic a(limit);
  const auto hessenberg = a.Round<Real>(HessenbergQuery(n), false);
  extent_t maximum = a.Product(complex ? 33 : 34, n);
  maximum = std::max(maximum, complex ? hessenberg : a.Sum({n, hessenberg}));
  if (vectors) {
    const auto trevc = a.Round<Real>(a.Product(n, 129));
    maximum = std::max(maximum, a.Sum({n, trevc}));
  }
  if (!complex) {
    maximum = std::max(maximum, a.Sum({n, 1}));
  }
  const auto returned = a.Round<Real>(maximum);
  return a.valid()
             ? Result<EigenQueryCounts>(EigenQueryCounts{maximum, returned})
             : Result<EigenQueryCounts>(Status(ErrorCode::kOverflow));
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_EIGEN_QUERY_COUNTS_H_
