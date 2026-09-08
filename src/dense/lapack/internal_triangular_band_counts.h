#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_triangular_band_counts {

// Pure source-integer admission. The caller separately validates enums,
// checked physical backing, placement, aliases and the actual native strides.
inline Status Solve(extent_t n, extent_t kd, extent_t nrhs, extent_t ldab,
                    extent_t ldb, DenseBlasTriangle triangle,
                    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
                    extent_t limit) {
  if (n < 0 || kd < 0 || nrhs < 0 || ldab <= kd || ldb < 1 || ldb < n ||
      limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // ASC zero-order completion neither narrows nor enters the foreign routine.
  if (n == 0) {
    return Status::Ok();
  }
  // TBTRS evaluates KD+1 before its quick return. The RHS loop has terminal
  // NRHS+1. Actual native LDAB/LDB are distinct from original ASC strides.
  if (n > limit || kd >= limit || nrhs >= limit || ldab > limit ||
      ldb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  // The nonunit diagonal scan executes even for zero RHS, through INFO=N+1.
  if (diagonal == DenseBlasDiagonal::kNonUnit && n == limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (nrhs == 0) {
    return Status::Ok();
  }
  const bool lower = triangle == DenseBlasTriangle::kLower;
  // Lower TBSV evaluates J+K before MIN(N,J+K), including the last column.
  // Both lower orientations evaluate J+1 even when that inner DO is empty.
  // Upper transpose traverses J=1..N and has terminal N+1. Upper N with
  // implicit diagonal only traverses descending controls through zero.
  if ((lower && kd > limit - n) ||
      ((lower || operation != DenseBlasTranspose::kNone) && n == limit)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_triangular_band_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_COUNTS_H_
