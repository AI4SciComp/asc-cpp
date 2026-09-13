#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_BAND_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_BAND_EXPERT_COUNTS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_lu_band_limits.h"

namespace asc::internal_lu_band_expert_counts {

// Pure source arithmetic; boundary tests use real integer maxima without
// constructing fictitious large backing spans. Foreign subscripts and loop
// cursors are validated separately from ASC's checked byte capacities.
inline Status CompactStorage(extent_t m, extent_t n, extent_t kl, extent_t ku,
                             extent_t ld, extent_t maximum) {
  if (m < 0 || n < 0 || kl < 0 || ku < 0 || ld < 1 || maximum < 127) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m > maximum || n > maximum || ku >= maximum || ld > maximum ||
      kl > maximum - ku - 1) {
    return Status(ErrorCode::kOverflow);
  }
  return ld < kl + ku + 1 ? Status(ErrorCode::kShape) : Status::Ok();
}

inline Status Equilibrate(extent_t m, extent_t n, extent_t kl, extent_t ku,
                          extent_t ld, extent_t maximum) {
  auto status = CompactStorage(m, n, kl, ku, ld, maximum);
  if (!status.ok() || m == 0 || n == 0) {
    return status;
  }
  // GBEQU's DO terminals need M+1/N+1. The scan forms J+KL and KD+I
  // before the subtraction in KD+I-J. Positive INFO's column domain is M+J.
  if (m == maximum || n == maximum || kl > maximum - n || m > maximum - n) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t greatest_row = std::min(m, n + kl);
  if (ku >= maximum - greatest_row) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status Condition(extent_t n, extent_t kl, extent_t ku, extent_t ld,
                        extent_t maximum) {
  auto status = internal_lu_band_limits::Storage(n, n, kl, ku, ld, maximum);
  if (!status.ok() || n == 0) {
    return status;
  }
  // Both LACN2 variants evaluate 3*N, independently of public WORK length.
  // Upper LATBS's scaled transpose path forms KD+I before subtracting JLEN.
  const extent_t kd = kl + ku;
  if (n > maximum / 3 || kd > maximum - std::min(kd, n - 1)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status Refine(extent_t n, extent_t kl, extent_t ku, extent_t ld,
                     extent_t ldf, extent_t nrhs, extent_t ldb, extent_t ldx,
                     extent_t maximum) {
  for (const auto& status :
       {CompactStorage(n, n, kl, ku, ld, maximum),
        internal_lu_band_limits::Storage(n, n, kl, ku, ldf, maximum)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (nrhs < 0 || ldb < std::max<extent_t>(1, n) ||
      ldx < std::max<extent_t>(1, n)) {
    return Status(ErrorCode::kShape);
  }
  // Empty N still executes the J=1,NRHS FERR/BERR loop.
  if (nrhs >= maximum || ldb > maximum || ldx > maximum || n > maximum / 3) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  // NZ forms KL+KU+2 before MIN; residual loops form K+KL. GBMV
  // has unit vector strides and source subscript KU+1-J+I. LACN2
  // forms 3*N. All GBTRS calls here use one packed RHS with LDB=N.
  if (kl + ku > maximum - 2 || kl > maximum - n) {
    return Status(ErrorCode::kOverflow);
  }
  return internal_lu_band_limits::Solve(n, kl, ku, ldf, 1, n, maximum);
}

inline Status Driver(extent_t n, extent_t kl, extent_t ku, extent_t ld,
                     extent_t ldf, extent_t nrhs, extent_t ldb, extent_t ldx,
                     int mode, extent_t maximum) {
  for (const auto& status :
       {Refine(n, kl, ku, ld, ldf, nrhs, ldb, ldx, maximum),
        Condition(n, kl, ku, ldf, maximum),
        internal_lu_band_limits::Solve(n, kl, ku, ldf, nrhs, ldx, maximum)}) {
    if (!status.ok()) {
      return status;
    }
  }
  if (mode != 2) {
    const auto status =
        internal_lu_band_limits::Factor(n, n, kl, ku, ldf, maximum);
    if (!status.ok()) {
      return status;
    }
  }
  if (mode == 1) {
    const auto status = Equilibrate(n, n, kl, ku, ld, maximum);
    if (!status.ok()) {
      return status;
    }
  }
  if (n == 0) {
    return Status::Ok();
  }
  // LANGB 'M' and '1' form N+KU+1 before subtracting J. Upper
  // LANTB 'M' forms K+2, K=KL+KU; terminal row cursor is K+2.
  // Copies, LAQGB and LANGB infinity paths also form J+KL.
  if (ku >= maximum - n || kl + ku > maximum - 2 || kl > maximum - n) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_lu_band_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_BAND_EXPERT_COUNTS_H_
