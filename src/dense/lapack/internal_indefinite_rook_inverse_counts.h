#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_INVERSE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_INVERSE_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"

namespace asc::internal_indefinite_rook_inverse_counts {

// Upper inversion advances K to N+1. SY swaps use a positive LDA stride for
// at most N-2 entries and advance the BLAS cursor past the last entry. HE
// conjugates those entries individually; its BLAS vectors have unit strides.
inline Status Inverse(extent_t order, extent_t leading, bool hermitian,
                      extent_t limit) {
  namespace counts = internal_indefinite_counts;
  if (!counts::Supported(limit) || order < 0 || leading < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order >= limit || leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  return !hermitian && order > 2 ? counts::Cursor(order - 2, leading, limit)
                                 : Status::Ok();
}

}  // namespace asc::internal_indefinite_rook_inverse_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_INVERSE_COUNTS_H_
