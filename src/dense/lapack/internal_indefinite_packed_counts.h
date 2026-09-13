#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_COUNTS_H_
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_packed_counts {
// Pinned SPTRF/HPTRF form (N-1)*N before division for upper storage,
// and N*(N+1) unconditionally for lower storage. Upper IMAX <= N-1.
// For N>=5 these dominate the other packed products, 2*N and selected
// unit-stride SPR/HPR/SCAL/SWAP/IAMAX cursors, including postincrements
// and left-to-right additions. N<=4 has intermediates below either ABI limit.
// This is deliberately separate from packed Cholesky/TPSV arithmetic.
inline Status Factor(extent_t n, bool upper, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n < 5) {
    return Status::Ok();
  }
  if (n >= limit) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t multiplier = upper ? n - 1 : n + 1;
  return n > limit / multiplier ? Status(ErrorCode::kOverflow) : Status::Ok();
}
}  // namespace asc::internal_indefinite_packed_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_COUNTS_H_
