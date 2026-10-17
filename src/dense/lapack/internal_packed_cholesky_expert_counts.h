#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_EXPERT_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_packed_cholesky_condition_counts.h"
#include "internal_packed_cholesky_refinement_counts.h"

namespace asc::internal_packed_cholesky_expert_counts {

// PPSVX still factors and estimates condition when NRHS=0. Its COPY count
// evaluates N*(N+1) before division, while LAQSP/LAQHP, LANSP/LANHP and PPEQU
// use packed cursors covered by the two-orientation PPCON bound. Nonempty
// refinement additionally needs COUNT's terminal six. N=0 completes in ASC.
// These formula checks do not inspect descriptors, flags or numerical data.
inline Status Driver(extent_t n, extent_t nrhs, extent_t ldb, extent_t ldx,
                     extent_t limit) {
  if (n < 0 || nrhs < 0 || ldb < 1 || ldx < 1 || ldb < n || ldx < n ||
      limit < 3) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (nrhs >= limit || ldb > limit || ldx > limit) {
    return Status(ErrorCode::kOverflow);
  }
  Status status =
      internal_packed_cholesky_condition_counts::Condition(n, limit);
  if (!status.ok() || nrhs == 0) {
    return status;
  }
  return internal_packed_cholesky_refinement_counts::Refine(n, nrhs, ldb, ldx,
                                                            limit);
}

}  // namespace asc::internal_packed_cholesky_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_EXPERT_COUNTS_H_
