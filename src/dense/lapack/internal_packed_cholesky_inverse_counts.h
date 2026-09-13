#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_INVERSE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_INVERSE_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_triangular_counts.h"

namespace asc::internal_packed_cholesky_inverse_counts {

// PPTRI first calls nonunit TPTRI. Its upper p+1 bound also covers PPTRI's
// product cursors, SPR/HPR and scalar BLAS loops. Lower TPTRI evaluates
// n*(n+1) before division; this covers PPTRI's left-associated JJ+n-j+1
// (largest intermediate p+n), and both selected lower TPMV paths of order
// n-j. These paths use INCX=1 and have no data-dependent retry loops.
inline Status Inverse(extent_t n, DenseBlasTriangle triangle, extent_t limit) {
  if (triangle != DenseBlasTriangle::kUpper &&
      triangle != DenseBlasTriangle::kLower) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return internal_packed_triangular_counts::Inverse(n, triangle, limit);
}

}  // namespace asc::internal_packed_cholesky_inverse_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_INVERSE_COUNTS_H_
