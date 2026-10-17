#ifndef ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_CONDITION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_CONDITION_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_positive_tridiagonal_counts.h"

namespace asc::internal_positive_tridiagonal_condition_counts {
// Active PTCON loops and its unit-stride AMAX reduction form N+1. Native
// N=0/ANORM=0 quick returns precede all array loops and require no workspace.
inline Status Condition(extent_t n, bool positive_norm, extent_t limit) {
  auto status = internal_positive_tridiagonal_counts::Factor(n, limit);
  if (!status.ok()) {
    return status;
  }
  return positive_norm && n == limit ? Status(ErrorCode::kOverflow)
                                     : Status::Ok();
}
}  // namespace asc::internal_positive_tridiagonal_condition_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_CONDITION_COUNTS_H_
