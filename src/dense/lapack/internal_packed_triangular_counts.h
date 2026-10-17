#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_packed_triangular_counts {

// All dimensions here are abstract integers; no invented backing spans.
// Callers validate enums and the ASC packed descriptor independently.
inline bool PackedFits(extent_t n, extent_t margin, extent_t limit) {
  if (n < 0 || n >= limit || margin < 0 || margin > limit) {
    return false;
  }
  const extent_t left = n % 2 == 0 ? n / 2 : n;
  const extent_t right = n % 2 == 0 ? n + 1 : (n + 1) / 2;
  return left <= (limit - margin) / right;
}

inline Status Inverse(extent_t n, DenseBlasTriangle triangle, extent_t limit) {
  if (n < 0 || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  // Upper TPTRI/TPMV use packed cursors through p+1, p=n(n+1)/2.
  // Lower TPTRI evaluates n*(n+1) before division. This stronger bound
  // also covers lower JJ+n before subtraction and the terminal JC=-n.
  if (!PackedFits(n, 1, limit) ||
      (triangle == DenseBlasTriangle::kLower && n > limit / (n + 1))) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status Solve(extent_t n, extent_t nrhs, extent_t ldb,
                    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
                    DenseBlasTranspose operation, extent_t limit) {
  if (n < 0 || nrhs < 0 || ldb < 1 || ldb < n || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (n > limit || nrhs >= limit || ldb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  // With zero RHS, TPTRS still scans a nonunit diagonal. Its unit case
  // reads neither numeric operand and never enters TPSV.
  if (nrhs == 0 && diagonal == DenseBlasDiagonal::kUnit) {
    return Status::Ok();
  }
  const bool lower = triangle == DenseBlasTriangle::kLower;
  const extent_t margin =
      lower && diagonal == DenseBlasDiagonal::kNonUnit ? n : 1;
  if (!PackedFits(n, margin, limit)) {
    return Status(ErrorCode::kOverflow);
  }
  // Actual INCX=1 TPSV descending packed paths evaluate n*(n+1).
  // The forward paths use only p+1; transpose and conjugate transpose
  // have the same source-integer paths but distinct complex arithmetic.
  const bool descending = lower == (operation != DenseBlasTranspose::kNone);
  if (nrhs != 0 && descending && n > limit / (n + 1)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_packed_triangular_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_COUNTS_H_
