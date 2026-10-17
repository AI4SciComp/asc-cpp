#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_REFINEMENT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_REFINEMENT_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_expert_counts.h"
#include "internal_indefinite_packed_solve_counts.h"

namespace asc::internal_indefinite_packed_refinement_counts {
// RFS checks provider dimensions and actual effective B/X leading dimensions.
// Active residual loops need N+1 and packed cursors; inner TRS always receives
// one compact RHS, and LACN2 needs3N even on the complex2N WORK route. Original
// dense physical storage ranges are separately proved by checked descriptors.
inline Status Refine(extent_t n, extent_t nrhs, extent_t ldb, extent_t ldx,
                     extent_t limit) {
  auto dimensions =
      internal_indefinite_expert_counts::Refinement(n, nrhs, limit);
  if (!dimensions.ok()) {
    return dimensions;
  }
  if (ldb < 1 || ldx < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (ldb > limit || ldx > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  if (ldb < n || ldx < n) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return internal_indefinite_packed_solve_counts::Solve(n, 1, n, limit);
}
}  // namespace asc::internal_indefinite_packed_refinement_counts
#endif
