#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_ESTIMATION_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_ESTIMATION_LIMITS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_band_limits.h"

namespace asc::internal_band_estimation_limits {

inline Status CheckCondition(extent_t order, extent_t bandwidth,
                             extent_t leading_dimension,
                             DenseBlasTriangle triangle, bool active,
                             extent_t maximum) {
  auto status = internal_band_limits::CheckStorage(order, bandwidth,
                                                   leading_dimension, maximum);
  if (!status.ok() || order == 0 || !active) {
    return status;
  }
  // All four LACN2 sources form the INTEGER expression 3*N, including
  // complex routes whose caller WORK contains only 2*N complex entries.
  if (order > maximum / 3) {
    return Status(ErrorCode::kOverflow);
  }
  // Upper scaled LATBS forms KD+I-JLEN before accessing its final in-band
  // coordinate; lower TBSV forms J+KD before MIN(N,J+KD).
  const auto extra = triangle == DenseBlasTriangle::kUpper
                         ? std::min(bandwidth, order - 1)
                         : order;
  if (bandwidth > maximum - extra) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_band_estimation_limits

#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_ESTIMATION_LIMITS_H_
