#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_REFINEMENT_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_REFINEMENT_LIMITS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_band_limits.h"

namespace asc::internal_band_refinement_limits {
inline Status Check(extent_t order, extent_t bandwidth, extent_t original_ld,
                    extent_t factor_ld, extent_t rhs_count, extent_t rhs_ld,
                    extent_t solution_ld, DenseBlasTriangle triangle,
                    extent_t maximum) {
  auto status = internal_band_limits::CheckStorage(order, bandwidth,
                                                   original_ld, maximum);
  if (!status.ok()) {
    return status;
  }
  status = internal_band_limits::CheckSolve(
      order, bandwidth, factor_ld, rhs_count, rhs_ld, triangle, maximum);
  if (!status.ok()) {
    return status;
  }
  if (solution_ld < std::max<extent_t>(1, order)) {
    return Status(ErrorCode::kShape);
  }
  if (solution_ld > maximum || rhs_count == maximum) {
    // Even N=0 executes DO J=1,NRHS when clearing the estimates.
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0 || rhs_count == 0) {
    return Status::Ok();
  }
  // Every scalar source evaluates both operands of MIN(N+1,2*KD+2).
  // Real WORK and every scalar LACN2 route additionally evaluate 3*N.
  if (order > maximum / 3 || maximum < 2 || bandwidth > (maximum - 2) / 2) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}
}  // namespace asc::internal_band_refinement_limits
#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_REFINEMENT_LIMITS_H_
