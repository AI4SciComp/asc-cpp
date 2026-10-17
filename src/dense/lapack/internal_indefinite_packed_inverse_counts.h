#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_INVERSE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_INVERSE_COUNTS_H_
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_packed_inverse_counts {
// Both source triangles form N*(N+1) before division. This full product
// bounds swap products, packed cursor additions, and unit-stride BLAS
// postincrements; terminal lower KCNEXT can be negative but stays >= -2*N.
// Paired-pivot validation separately establishes all data-dependent indices.
inline Status Inverse(extent_t n, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (n >= limit) {
    return Status(ErrorCode::kOverflow);
  }
  return n > limit / (n + 1) ? Status(ErrorCode::kOverflow) : Status::Ok();
}
}  // namespace asc::internal_indefinite_packed_inverse_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_PACKED_INVERSE_COUNTS_H_
