#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_DRIVER_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_DRIVER_LIMITS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_band_estimation_limits.h"
#include "internal_band_limits.h"
#include "internal_band_refinement_limits.h"

namespace asc::internal_band_driver_limits {

// Actual PBSVX source INTEGER closure, not a numerical workspace query.
// FACT is already restricted to the three typed public routes by the caller.
inline Status Check(extent_t order, extent_t bandwidth, extent_t original_ld,
                    extent_t factor_ld, extent_t rhs_count, extent_t rhs_ld,
                    extent_t solution_ld, DenseBlasTriangle triangle,
                    char factor_mode, bool complex_scalar, extent_t maximum) {
  auto status = internal_band_refinement_limits::Check(
      order, bandwidth, original_ld, factor_ld, rhs_count, rhs_ld, solution_ld,
      triangle, maximum);
  if (!status.ok()) {
    return status;
  }
  if (factor_mode != 'F') {
    status = internal_band_limits::CheckFactor(order, bandwidth, factor_ld,
                                               triangle, true, maximum);
    if (!status.ok()) {
      return status;
    }
  }
  // Unlike refinement alone, PBSVX estimates condition for NRHS=0 too.
  status = internal_band_estimation_limits::CheckCondition(
      order, bandwidth, factor_ld, triangle, true, maximum);
  if (!status.ok() || order == 0) {
    return status;
  }
  if (factor_mode == 'E' && triangle == DenseBlasTriangle::kUpper) {
    // Real LAQSB evaluates KD+1+I-J through I=J=N. Complex LAQHB's
    // off-diagonal loop only reaches I=N-1; its N=1 path uses KD+1 alone.
    const extent_t extra = complex_scalar ? order : order + 1;
    if (bandwidth > maximum - extra) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return Status::Ok();
}

}  // namespace asc::internal_band_driver_limits
#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_DRIVER_LIMITS_H_
