#ifndef ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_EXPERT_COUNTS_H_
#include "internal_positive_tridiagonal_refinement_counts.h"
namespace asc::internal_positive_tridiagonal_expert_counts {
// PTSVX forms INFO=N+1 even for zero RHS; active refinement uses PTRFS bounds.
inline Status Solve(extent_t n, extent_t nrhs, extent_t ldb, bool complex,
                    extent_t limit) {
  auto status = internal_positive_tridiagonal_refinement_counts::Refine(
      n, nrhs, ldb, n == 0 ? 1 : n, complex, limit);
  if (!status.ok()) {
    return status;
  }
  return n == limit ? Status(ErrorCode::kOverflow) : Status::Ok();
}
}  // namespace asc::internal_positive_tridiagonal_expert_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_EXPERT_COUNTS_H_
