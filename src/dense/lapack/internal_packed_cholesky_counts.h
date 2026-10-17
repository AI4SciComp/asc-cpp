#ifndef ASC_SRC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_COUNTS_H_
#define ASC_SRC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_packed_cholesky_counts {

// Exact S/D/C/Z PPTRF integer admission for the pinned selected call paths.
// Upper uses only transpose/conjugate-upper TPSV, whose packed cursor starts
// at one; its unused reverse path's N*(N+1) is not a reason to reject input.
// Lower evaluates JJ+N-J+1 left to right. The largest JJ+N is p+N-2 at J=N-1.
// Scalar BLAS cleanup/unrolled DO controls fit under these packed bounds.
inline Status Factor(extent_t n, DenseBlasTriangle triangle, extent_t limit) {
  if (n < 0 || limit < 3 ||
      (triangle != DenseBlasTriangle::kUpper &&
       triangle != DenseBlasTriangle::kLower)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (n >= limit) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t first = n % 2 == 0 ? n / 2 : n;
  const extent_t second = n % 2 == 0 ? n + 1 : (n + 1) / 2;
  if (first > limit / second) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t packed = first * second;
  if (triangle == DenseBlasTriangle::kLower && n > 1 &&
      packed > limit - (n - 2)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_packed_cholesky_counts

#endif  // ASC_SRC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_COUNTS_H_
