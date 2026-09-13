#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_triangular_counts {

// Pure source-integer checks: no descriptors with invented backing spans.
// Pin: TRTRI/TRTI2/TRTRS and Reference BLAS at
// 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca.
inline Status Inverse(extent_t n, extent_t lda, DenseBlasTriangle triangle,
                      bool blocked, extent_t limit) {
  if (n < 0 || lda < 1 || lda < n) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (n > limit || lda > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (blocked) {
    // ILAENV(1,*TRTRI,...) selects 64 for every S/D/C/Z route. Upper
    // DO J=1,N,64 evaluates the terminal J+64; lower evaluates J+JB<=N.
    constexpr extent_t kBlock = 64;
    if (n > kBlock && triangle == DenseBlasTriangle::kUpper) {
      const extent_t last = 1 + ((n - 1) / kBlock) * kBlock;
      if (last > limit - kBlock) {
        return Status(ErrorCode::kOverflow);
      }
    }
    // TRTRI's nonunit INFO=1,N loop and lower J+JB require N+1. This
    // remains sufficient for its TRMM/TRSM suborders and all block TRTI2.
    if (n == limit) {
      return Status(ErrorCode::kOverflow);
    }
  } else if (triangle == DenseBlasTriangle::kUpper && n == limit) {
    // Upper TRTI2's DO J=1,N has terminal N+1. Lower descends to zero;
    // its TRMV/SCAL suborders are at most N-1, so N==limit is legal.
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status Solve(extent_t n, extent_t nrhs, extent_t lda, extent_t ldb,
                    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
                    DenseBlasTranspose operation, extent_t limit) {
  if (n < 0 || nrhs < 0 || lda < 1 || ldb < 1 || lda < n || ldb < n) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (n > limit || nrhs > limit || lda > limit || ldb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  // NRHS=0 still enters TRTRS's nonunit diagonal loop. Unit NRHS=0
  // reaches TRSM's zero-column return and reads neither A nor B.
  if (n == limit && diagonal == DenseBlasDiagonal::kNonUnit) {
    return Status(ErrorCode::kOverflow);
  }
  if (nrhs == 0) {
    return Status::Ok();
  }
  // All active left TRSM paths advance J=1,NRHS. With alpha exactly one,
  // only upper/no-transpose avoids an N+1 loop terminal or I+1 bound.
  if (nrhs == limit ||
      (n == limit && (triangle != DenseBlasTriangle::kUpper ||
                      operation != DenseBlasTranspose::kNone))) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_triangular_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_COUNTS_H_
