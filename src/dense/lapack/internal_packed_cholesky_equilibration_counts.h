#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_EQUILIBRATION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_EQUILIBRATION_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_cholesky_counts.h"

namespace asc::internal_packed_cholesky_equilibration_counts {

// PPEQU reads only real diagonal components. Row-upper diagonal positions
// equal column-lower positions (and conversely), so exchanging native UPLO
// permits direct borrowed storage without packing or off-diagonal reads.
// Upper JJ+=I is bounded by p; lower left-associated JJ+N-I+2 needs p+N-2.
// These are exactly the admitted PPTRF cursor bounds; N+1 covers scale loops.
inline Status Equilibrate(extent_t n, DenseBlasTriangle triangle,
                          DenseBlasLayout layout, extent_t limit) {
  if ((triangle != DenseBlasTriangle::kUpper &&
       triangle != DenseBlasTriangle::kLower) ||
      (layout != DenseBlasLayout::kRowMajor &&
       layout != DenseBlasLayout::kColumnMajor)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (layout == DenseBlasLayout::kRowMajor) {
    triangle = triangle == DenseBlasTriangle::kUpper
                   ? DenseBlasTriangle::kLower
                   : DenseBlasTriangle::kUpper;
  }
  return internal_packed_cholesky_counts::Factor(n, triangle, limit);
}

}  // namespace asc::internal_packed_cholesky_equilibration_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_EQUILIBRATION_COUNTS_H_
