#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_EXPERT_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_triangular_expert_counts {

// Pure checks accept an explicit native limit so both integer ABIs can be
// exercised without allocating near-limit arrays. Inactive calls stay in ASC.
inline Status Active(extent_t n, extent_t nrhs, extent_t lda, extent_t ldb,
                     extent_t ldx, extent_t limit) {
  if (n < 0 || nrhs < 0 || lda < 1 || ldb < 1 || ldx < 1 || limit < 3) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  // LACN2 evaluates 3*N; real WORK uses 2*N+I. This also bounds N+1,
  // ascending/descending triangular loops and all unit-stride BLAS cursors.
  // TRRFS's outer DO J=1,NRHS needs its terminal NRHS+1 representable.
  if (n > limit / 3 || nrhs >= limit || lda > limit || ldb > limit ||
      ldx > limit) {
    return Status(ErrorCode::kOverflow);
  }
  return lda < n || ldb < n || ldx < n ? Status(ErrorCode::kInvalidArgument)
                                       : Status::Ok();
}

}  // namespace asc::internal_triangular_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_EXPERT_COUNTS_H_
