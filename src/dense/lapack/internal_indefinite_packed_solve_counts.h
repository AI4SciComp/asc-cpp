#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_SOLVE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_SOLVE_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"

namespace asc::internal_indefinite_packed_solve_counts {

// SPTRS/HPTRS form N*(N+1) before division in their packed cursors.
// That full product also bounds KC+N and the paired-block cursor updates;
// the independently checked strided RHS BLAS cursors finish at 1+NRHS*LDB.
// Checked empty/zero-RHS calls never enter these foreign loops.
inline Status Solve(extent_t order, extent_t rhs_columns, extent_t leading,
                    extent_t limit) {
  if (order < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  auto rhs_counts =
      internal_indefinite_counts::Solve(order, rhs_columns, leading, limit);
  if (!rhs_counts.ok() || order == 0 || rhs_columns == 0) {
    return rhs_counts;
  }
  // Keep the divisor proof local even when a caller analyzes the helper
  // without inlining the shared RHS validation.
  if (order >= limit) {
    return Status(ErrorCode::kOverflow);
  }
  return order > limit / (order + 1) ? Status(ErrorCode::kOverflow)
                                     : Status::Ok();
}

}  // namespace asc::internal_indefinite_packed_solve_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_SOLVE_COUNTS_H_
