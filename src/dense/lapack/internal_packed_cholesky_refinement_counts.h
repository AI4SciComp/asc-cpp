#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_REFINEMENT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_REFINEMENT_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_packed_triangular_expert_counts.h"

namespace asc::internal_packed_cholesky_refinement_counts {

// PPRFS uses LACN2's 3*N, original packed residual cursors through p+1,
// and PPTRS's two TPSV orientations with predivision N*(N+1). The common
// estimator admission also binds actual B/X strides and the NRHS+1 terminal.
// COUNT starts one and increments after each of at most ITMAX=5 corrections;
// its terminal six matters for the pure virtual-limit source oracle as well.
inline Status Refine(extent_t n, extent_t nrhs, extent_t ldb, extent_t ldx,
                     extent_t limit) {
  Status status = internal_packed_triangular_expert_counts::Active(n, nrhs, ldb,
                                                                   ldx, limit);
  if (!status.ok() || n == 0 || nrhs == 0) {
    return status;
  }
  return limit < 6 ? Status(ErrorCode::kOverflow) : Status::Ok();
}

}  // namespace asc::internal_packed_cholesky_refinement_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_REFINEMENT_COUNTS_H_
