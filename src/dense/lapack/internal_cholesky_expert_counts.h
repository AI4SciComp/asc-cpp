#ifndef ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_EXPERT_COUNTS_H_

#include <cstdint>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_cholesky_expert_counts {

inline bool Supported(extent_t limit) {
  return limit == std::numeric_limits<std::int32_t>::max() ||
         limit == std::numeric_limits<std::int64_t>::max();
}

// POEQU[B]'s unit-step output loop finishes with I=N+1.
inline Status Equilibration(extent_t order, extent_t limit) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return order < 0 || order >= limit ? Status(ErrorCode::kOverflow)
                                     : Status::Ok();
}

// Both xLACN2 variants evaluate provider INTEGER 3*N, even though the
// complex estimator's primary WORK requires only 2*N entries.
inline Status Estimator(extent_t order, extent_t limit) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return order < 0 || order > limit / 3 ? Status(ErrorCode::kOverflow)
                                        : Status::Ok();
}

// Wrappers handle empty problems without foreign loops. Active PORFS/POSVX
// have DO J=1,NRHS; all N+1,2*N+1 and blocked POTRF endpoints are dominated
// by LACN2's 3*N bound for the supported provider integer widths.
inline Status System(extent_t order, extent_t rhs_columns, extent_t limit,
                     bool factor_or_estimate) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order < 0 || rhs_columns < 0 || order > limit || rhs_columns > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0 || (!factor_or_estimate && rhs_columns == 0)) {
    return Status::Ok();
  }
  if (rhs_columns == limit) {
    return Status(ErrorCode::kOverflow);
  }
  return Estimator(order, limit);
}

}  // namespace asc::internal_cholesky_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_EXPERT_COUNTS_H_
