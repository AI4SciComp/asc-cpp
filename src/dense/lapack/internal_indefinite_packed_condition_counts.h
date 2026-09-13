#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_CONDITION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_CONDITION_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
#include "internal_indefinite_expert_counts.h"
#include "internal_indefinite_packed_solve_counts.h"

namespace asc::internal_indefinite_packed_condition_counts {
// Active CON enters packed TRS with one compact RHS and LACN2 with 2*N WORK.
// Prove full packed products/BLAS endpoints and the estimator's 3*N expression.
// Empty and zero-norm checked noncalls never form these native expressions.
inline Status Condition(extent_t n, bool zero_norm, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0 || zero_norm) {
    return Status::Ok();
  }
  auto solve = internal_indefinite_packed_solve_counts::Solve(n, 1, n, limit);
  if (!solve.ok()) {
    return solve;
  }
  return internal_indefinite_expert_counts::Estimator(n, limit);
}
}  // namespace asc::internal_indefinite_packed_condition_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_CONDITION_COUNTS_H_
