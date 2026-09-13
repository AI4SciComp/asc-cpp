#ifndef ASC_DENSE_LAPACK_INTERNAL_BAND_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_BAND_LIMITS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_band_limits {

// Number of leading diagonals whose imaginary components the actual complex
// factor route overwrites. Inputs are checked source dimensions and raw INFO.
// Clamp additions by subtraction so this pure calculation also handles the
// full ASC extent range without constructing fictitious matrix backing.
inline extent_t NormalizedDiagonalPrefix(extent_t order, extent_t bandwidth,
                                         bool blocked, extent_t info) {
  if (order < 0 || bandwidth < 0 || info < 0 || info > order) {
    return 0;
  }
  if (info == 0) {
    return order;
  }
  if (!blocked || bandwidth <= 64) {
    const extent_t previous = info - 1;
    if (previous == 0) {
      return info;
    }
    // PBTF2's previous HER normalizes the whole update diagonal, even
    // for an identically zero vector; the failed pivot itself is also written.
    return std::max(info, previous + std::min(bandwidth, order - previous));
  }
  const extent_t block_start = 1 + 32 * ((info - 1) / 32);
  if (block_start == 1) {
    return info;
  }
  // The preceding complete PBTRF block HERK-updates both A22 and A33,
  // a contiguous region ending at min(order, block_start+bandwidth-1).
  // POTF2 in the failed current block itself writes through INFO only.
  const extent_t before_block = block_start - 1;
  return std::max(info,
                  before_block + std::min(bandwidth, order - before_block));
}

// Pure integer checks permit exact boundary tests without fabricated backing.
inline Status CheckStorage(extent_t order, extent_t bandwidth,
                           extent_t leading_dimension, extent_t maximum) {
  if (order < 0 || bandwidth < 0 || leading_dimension < 1 || maximum < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // Every source evaluates KD+1 even before a zero-order quick return.
  if (order > maximum || bandwidth >= maximum || leading_dimension > maximum) {
    return Status(ErrorCode::kOverflow);
  }
  if (leading_dimension <= bandwidth) {
    return Status(ErrorCode::kShape);
  }
  return Status::Ok();
}

inline Status CheckFactor(extent_t order, extent_t bandwidth,
                          extent_t leading_dimension,
                          DenseBlasTriangle triangle, bool blocked,
                          extent_t maximum) {
  auto status = CheckStorage(order, bandwidth, leading_dimension, maximum);
  if (!status.ok() || order == 0) {
    return status;
  }
  // Exact pinned ILAENV PB/TRF: NB=1 for KD<=64, otherwise NB=32. Account
  // for the final DO I=1,N,NB update, not only the last accessed matrix row.
  const extent_t step = blocked && bandwidth > 64 ? 32 : 1;
  if (step > maximum - 1 || (order - 1) / step > (maximum - 1 - step) / step) {
    return Status(ErrorCode::kOverflow);
  }
  // Upper PBTF2 passes KLD to SCAL/SYR/HER/LACGV; their integer vector
  // cursors include the final 1+length*KLD update. Blocked PBTRF calls POTF2
  // on at most 32 rows, whose strided DOT/GEMV vectors have length <=IB-1.
  extent_t length = 0;
  if (step == 32) {
    length = std::min<extent_t>(order, 32) - 1;
  } else if (triangle == DenseBlasTriangle::kUpper) {
    length = std::min(bandwidth, order - 1);
  }
  const auto increment = std::max<extent_t>(1, leading_dimension - 1);
  if (length != 0 && increment > (maximum - 1) / length) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status CheckSolve(extent_t order, extent_t bandwidth,
                         extent_t leading_dimension, extent_t rhs_count,
                         extent_t rhs_leading_dimension,
                         DenseBlasTriangle triangle, extent_t maximum) {
  auto status = CheckStorage(order, bandwidth, leading_dimension, maximum);
  if (!status.ok()) {
    return status;
  }
  if (rhs_count < 0 || rhs_leading_dimension < std::max<extent_t>(1, order)) {
    return Status(ErrorCode::kShape);
  }
  if (rhs_count > maximum || rhs_leading_dimension > maximum) {
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0 || rhs_count == 0) {
    return Status::Ok();
  }
  // PBTRS loops through RHS, TBSV through rows. Lower TBSV evaluates J+KD
  // before MIN(N,J+KD), including J=N; upper TBSV only subtracts KD.
  if (order == maximum || rhs_count == maximum ||
      (triangle == DenseBlasTriangle::kLower && order > maximum - bandwidth)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_band_limits

#endif  // ASC_DENSE_LAPACK_INTERNAL_BAND_LIMITS_H_
