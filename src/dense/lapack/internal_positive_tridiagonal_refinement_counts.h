#ifndef ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_REFINEMENT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_REFINEMENT_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_tridiagonal_counts.h"

namespace asc::internal_positive_tridiagonal_refinement_counts {
// Native active loops form N+1/NRHS+1. Real PTRFS additionally indexes WORK at
// N+N; its residual correction uses PTTRS(N,1,...,LDB=N). ASC local empty
// completion executes none of these native loops. Layout bytes are separate.
inline Status Refine(extent_t n, extent_t nrhs, extent_t ldb, extent_t ldx,
                     bool complex, extent_t limit) {
  if (!internal_tridiagonal_counts::Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n < 0 || n > limit || nrhs < 0 || nrhs > limit || ldb < 1 ||
      ldb > limit || ldx < 1 || ldx > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  return nrhs == limit || n == limit || (!complex && n > limit / 2)
             ? Status(ErrorCode::kOverflow)
             : Status::Ok();
}
}  // namespace asc::internal_positive_tridiagonal_refinement_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_REFINEMENT_COUNTS_H_
