#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_EXPERT_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_EXPERT_LIMITS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_band_limits.h"

namespace asc::internal_band_expert_limits {

// Pure source-integer checks; no fabricated matrix addresses or allocations.
inline Status CheckEquilibration(extent_t order, extent_t bandwidth,
                                 extent_t leading_dimension, extent_t maximum) {
  auto status = internal_band_limits::CheckStorage(order, bandwidth,
                                                   leading_dimension, maximum);
  if (!status.ok()) {
    return status;
  }
  // PBEQU's final DO I=1,N cursor advances once beyond N on nonempty paths.
  return order == maximum ? Status(ErrorCode::kOverflow) : Status::Ok();
}

inline Status CheckSimpleDriver(extent_t order, extent_t bandwidth,
                                extent_t leading_dimension, extent_t rhs_count,
                                extent_t rhs_leading_dimension,
                                DenseBlasTriangle triangle, extent_t maximum) {
  // The actual PBSV driver factors even with NRHS=0. Its PBTRF route is
  // blocked according to pinned ILAENV; PBTRS then has its own early return.
  auto status = internal_band_limits::CheckFactor(
      order, bandwidth, leading_dimension, triangle, true, maximum);
  if (!status.ok()) {
    return status;
  }
  return internal_band_limits::CheckSolve(order, bandwidth, leading_dimension,
                                          rhs_count, rhs_leading_dimension,
                                          triangle, maximum);
}

}  // namespace asc::internal_band_expert_limits

#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_EXPERT_LIMITS_H_
