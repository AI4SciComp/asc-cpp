#ifndef ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_tridiagonal_counts.h"

namespace asc::internal_positive_tridiagonal_counts {

// PTTRF's four-way loop and its cleanup stop with I<=N; N-1/N-4 are signed.
inline Status Factor(extent_t n, extent_t limit) {
  if (!internal_tridiagonal_counts::Supported(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return n < 0 || n > limit ? Status(ErrorCode::kOverflow) : Status::Ok();
}

// Pinned ILAENV returns NB=1 for PTTRS. Its outer terminal J is NRHS+1;
// PTTS2's active forward loops end at N+1. For N=1, each PTTS2 calls SCAL
// with one element and increment LDB: its nonunit terminal is 1+LDB.
// These are native INTEGER operations, not invented n*LDB workspace counts.
inline Status Solve(extent_t n, extent_t nrhs, extent_t leading,
                    extent_t limit) {
  Status status = Factor(n, limit);
  if (!status.ok()) {
    return status;
  }
  if (nrhs < 0 || nrhs > limit || leading < 1 || leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  return n == limit || nrhs == limit || (n == 1 && leading == limit)
             ? Status(ErrorCode::kOverflow)
             : Status::Ok();
}

}  // namespace asc::internal_positive_tridiagonal_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_POSITIVE_TRIDIAGONAL_COUNTS_H_
