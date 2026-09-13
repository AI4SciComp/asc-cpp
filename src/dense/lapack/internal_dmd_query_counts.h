#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_QUERY_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_QUERY_COUNTS_H_

#include <algorithm>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_dmd_counts.h"
#include "internal_dmd_divide_query_counts.h"
#include "internal_dmd_eigen_query_counts.h"
#include "internal_dmd_preconditioned_query_counts.h"
#include "internal_dmd_svd_query_counts.h"

namespace asc::internal_dmd_counts {

// Metadata-only resource query predecessor. Native execution additionally
// needs selected downstream address, iteration and returned-rank validation.
struct QueryCounts {
  MinimumCounts minimum;
  extent_t raw_preferred;
  extent_t query_preferred;
  extent_t capacity_preferred;
};

template <typename Real>
Result<SvdQueryCounts> SelectedSvdQuery(extent_t m, extent_t n, int svd,
                                        bool complex, extent_t limit) {
  if (svd == 1) {
    return SvdQuery<Real>(m, n, complex, true, limit);
  }
  if (svd == 2) {
    return DivideQuery<Real>(m, n, complex, limit);
  }
  return PreconditionedQuery<Real>(m, n, complex, svd == 4, limit);
}

template <typename Real>
Result<QueryCounts> Query(extent_t m, extent_t n, int svd, bool vectors,
                          bool complex, extent_t limit) {
  const auto minimum = Minimum<Real>(m, n, svd, vectors, complex, limit);
  if (!minimum.ok()) {
    return minimum.status();
  }
  if (n == 0) {
    return QueryCounts{*minimum, 2, 2, 2};
  }
  Arithmetic a(limit);
  extent_t preferred = minimum->source_scalar;
  if (svd != 4 || complex) {
    const auto nested = SelectedSvdQuery<Real>(m, n, svd, complex, limit);
    if (!nested.ok()) {
      return nested.status();
    }
    const auto suffix = !complex && svd == 3
                            ? a.Round<Real>(std::max<extent_t>(m, 2), false)
                            : 0;
    const auto prefix = complex ? extent_t{0} : n;
    const auto nested_preferred =
        a.Sum({prefix, nested->returned_preferred, suffix});
    // S's SVDQ and C/Z's SVD/Q/JSV branches use the returned preferred
    // directly. Keep that representation distinct from sufficient capacity.
    if ((complex && svd != 2) ||
        (!complex && svd == 3 && std::is_same_v<Real, float>)) {
      preferred = std::max<extent_t>(2, nested_preferred);
    } else {
      preferred = std::max(preferred, nested_preferred);
    }
  }
  const auto eigen = EigenQuery<Real>(n, complex, vectors, limit);
  if (!eigen.ok()) {
    return eigen.status();
  }
  preferred =
      std::max(preferred, a.Sum({complex ? 0 : n, eigen->returned_preferred}));
  const auto encoded = a.Round<Real>(preferred, false);
  return a.valid() ? Result<QueryCounts>(QueryCounts{
                         *minimum, preferred, encoded,
                         std::max({minimum->scalar, preferred, encoded})})
                   : Result<QueryCounts>(Status(ErrorCode::kOverflow));
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_QUERY_COUNTS_H_
