#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_COUNTS_H_

#include <cstdint>
#include <limits>

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_tridiagonal_counts {

inline bool Supported(extent_t limit) {
  return limit == std::numeric_limits<std::int32_t>::max() ||
         limit == std::numeric_limits<std::int64_t>::max();
}

// Pure source INTEGER arithmetic: no fake backing is required by boundary
// tests. GTTRF initializes/checks pivots using DO I=1,N, including final I=N+1.
inline Status Factor(extent_t order, extent_t limit) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return order < 0 || order >= limit ? Status(ErrorCode::kOverflow)
                                     : Status::Ok();
}

// Pinned ILAENV returns NB=1 for GTTRS; its multiple-RHS DO J terminal is
// NRHS+1. All solves consume GTTRF factors, whose valid source order is <limit.
// GTTS2 does not pass matrix rows to strided BLAS; no n*LDB cursor is invented.
inline Status Solve(extent_t order, extent_t rhs, extent_t limit) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order < 0 || rhs < 0 || order > limit || rhs > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0 || rhs == 0) {
    return Status::Ok();
  }
  return order == limit || rhs == limit ? Status(ErrorCode::kOverflow)
                                        : Status::Ok();
}

// GTSV forward elimination ends at N-1; its reverse solve ends at zero, so
// order==limit is not artificially excluded. Active RHS DO terminals need
// rhs<limit. Real rhs==0 uses explicit n-entry caller surrogate storage.
inline Status Driver(extent_t order, extent_t rhs, extent_t limit) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order < 0 || rhs < 0 || order > limit || rhs > limit ||
      (order != 0 && rhs == limit)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

// Both actual real and complex LACN2 evaluate INTEGER 3*N. This also covers
// WORK offsets N+1/2*N+1, unit-increment BLAS cursors and GTTRF's N+1.
inline Status Estimator(extent_t order, extent_t limit) {
  if (!Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return order < 0 || order > limit / 3 ? Status(ErrorCode::kOverflow)
                                        : Status::Ok();
}

inline Status Expert(extent_t order, extent_t rhs, bool estimate_without_rhs,
                     extent_t limit) {
  Status valid = Driver(order, rhs, limit);
  if (!valid.ok() || order == 0 || (!estimate_without_rhs && rhs == 0)) {
    return valid;
  }
  return Estimator(order, limit);
}

}  // namespace asc::internal_tridiagonal_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIDIAGONAL_COUNTS_H_
