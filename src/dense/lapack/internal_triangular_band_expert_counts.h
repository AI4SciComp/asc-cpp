#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_EXPERT_COUNTS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_triangular_band_expert_counts {

// Private source-integer admission. The caller validates enums, full backing,
// placement and metadata aliases separately and passes actual foreign strides.
inline Status Condition(extent_t n, extent_t kd, extent_t ldab,
                        DenseBlasTriangle triangle, bool one_norm,
                        extent_t limit) {
  if (n < 0 || kd < 0 || ldab <= kd || limit < 3) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  // LACN2 evaluates 3*N even for complex WORK. This bounds 2*N+1, all
  // vector and N+1 terminals. Actual physical addresses are checked in ASC.
  if (n > limit / 3 || kd >= limit || ldab > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (triangle == DenseBlasTriangle::kUpper) {
    // LANTB's one-norm evaluates K+2-J before entering its inner DO, even
    // for a unit scalar matrix. LATBS's slow transpose loop evaluates
    // KD+I-JLEN left to right, with I<=JLEN<=min(KD,N-1).
    if ((one_norm && kd > limit - 2) || kd > limit - std::min(kd, n - 1)) {
      return Status(ErrorCode::kOverflow);
    }
  } else if (kd > limit - n) {
    // Lower TBSV and the infinity norm evaluate J+KD before MIN(N,J+KD).
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status Errors(extent_t n, extent_t kd, extent_t nrhs, extent_t ldab,
                     extent_t ldb, extent_t ldx, DenseBlasTriangle triangle,
                     DenseBlasDiagonal diagonal, extent_t limit) {
  if (n < 0 || kd < 0 || nrhs < 0 || ldab <= kd || ldb < 1 || ldx < 1 ||
      limit < 3 || (n != 0 && nrhs != 0 && (ldb < n || ldx < n))) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // ASC publishes local zero error bounds without entering or narrowing
  // unused foreign arguments on zero-order/zero-RHS execution.
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  // LACN2's 3*N, the outer NRHS+1 terminal, and TBRFS's NZ=KD+2.
  if (n > limit / 3 || nrhs >= limit || kd > limit - 2 || ldab > limit ||
      ldb > limit || ldx > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (triangle == DenseBlasTriangle::kUpper) {
    // AB(KD+1+I-K,K) forms KD+1+I before subtraction. Nonunit reaches
    // I=N; unit reaches N-1. NZ already bounds the empty unit N=1 case.
    const extent_t extra = diagonal == DenseBlasDiagonal::kNonUnit ? 1 : 0;
    if (kd > limit - n - extra) {
      return Status(ErrorCode::kOverflow);
    }
  } else if (kd > limit - n) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_triangular_band_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_BAND_EXPERT_COUNTS_H_
