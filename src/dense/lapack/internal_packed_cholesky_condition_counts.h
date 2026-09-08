#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_CONDITION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_CONDITION_COUNTS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_packed_triangular_expert_counts.h"

namespace asc::internal_packed_cholesky_condition_counts {

// PPCON's two LATPS calls use opposite transpose orientations and unit-stride
// vectors. The existing packed estimator bound covers both actual n*(n+1)
// predivision expressions, terminal p+n+1 cursors, and LACN2's 3*n. The real
// WORK(2*n+1) CNORM segment ends at 3*n; complex WORK has two n-entry segments.
// This pure bound is independent of ANORM and does not inspect factor storage.
inline Status Condition(extent_t n, extent_t limit) {
  const extent_t leading = std::max<extent_t>(1, n);
  return internal_packed_triangular_expert_counts::Active(n, 1, leading,
                                                          leading, limit);
}

}  // namespace asc::internal_packed_cholesky_condition_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_CONDITION_COUNTS_H_
