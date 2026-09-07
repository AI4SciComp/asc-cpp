#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_BAND_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_BAND_LIMITS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_lu_band_limits {

// Pure source arithmetic checks: tests never fabricate enormous backing spans.
// The selected provider uses a signed 32- or 64-bit INTEGER, hence
// maximum>=127.
inline Status Storage(extent_t m, extent_t n, extent_t kl, extent_t ku,
                      extent_t ld, extent_t maximum) {
  if (m < 0 || n < 0 || kl < 0 || ku < 0 || ld < 1 || maximum < 127) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // GBTRF first forms KV=KU+KL; both routines test LDAB>=2*KL+KU+1
  // before their quick return. Check the actual source expression in order.
  if (m > maximum || n > maximum || ku >= maximum || ld > maximum ||
      kl > (maximum - ku - 1) / 2) {
    return Status(ErrorCode::kOverflow);
  }
  return ld < 2 * kl + ku + 1 ? Status(ErrorCode::kShape) : Status::Ok();
}

inline Status Factor(extent_t m, extent_t n, extent_t kl, extent_t ku,
                     extent_t ld, extent_t maximum) {
  auto status = Storage(m, n, kl, ku, ld, maximum);
  if (!status.ok() || m == 0 || n == 0) {
    return status;
  }
  const extent_t count = std::min(m, n);
  const extent_t kv = kl + ku;
  // ILAENV receives N4=KU, not KL. GBTRF then rejects NB>KL.
  const extent_t step = ku > 64 && kl >= 32 ? 32 : 1;
  // Final DO cursor 1+step*ceil(min(m,n)/step), initial KU+2,
  // and J+KV / J+KU+JP (before -1). Blocked scalar expressions evaluate
  // KV+1+JJ before -J and local-JJ+J+KV before -1.
  if (ku > maximum - 2 || count > maximum - kv - 1 ||
      (count - 1) / step > (maximum - 1 - step) / step) {
    return Status(ErrorCode::kOverflow);
  }
  if (step == 32) {
    // KV+JP+JJ and KV+1+IP are evaluated before subtracting J/JJ.
    // The greatest possible pivot row is min(m,min(m,n)+KL).
    const extent_t pivot_row = count + std::min(kl, m - count);
    if (pivot_row > maximum - kv - 1) {
      return Status(ErrorCode::kOverflow);
    }
  }
  if (kl != 0 && m > 1) {
    // GBTF2 SWAP length JU-J+1 is bounded by the first step's maximal
    // band reach. Blocked SWAP lengths <=NB are included in that bound.
    // Reference SWAP/GER advance their nonunit INTEGER cursors after the
    // final access: checking only (length-1)*increment would be insufficient.
    const extent_t length = std::min(n, ku + std::min(kl, m - 1) + 1);
    if (ld - 1 > (maximum - 1) / length) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return Status::Ok();
}

inline Status Solve(extent_t n, extent_t kl, extent_t ku, extent_t ld,
                    extent_t nrhs, extent_t ldb, extent_t maximum) {
  auto status = Storage(n, n, kl, ku, ld, maximum);
  if (!status.ok()) {
    return status;
  }
  if (nrhs < 0 || ldb < std::max<extent_t>(1, n)) {
    return Status(ErrorCode::kShape);
  }
  if (nrhs > maximum || ldb > maximum) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  // GBTRS's RHS loop and upper TBSV's row loop have terminal +1 cursors.
  // Only the upper TBSV branch is called; its bandwidth expressions subtract
  // K from J (unlike lower TBSV's potentially overflowing J+K).
  if (n == maximum || nrhs == maximum) {
    return Status(ErrorCode::kOverflow);
  }
  if (kl != 0 && n > 1 && ldb > (maximum - 1) / nrhs) {
    // SWAP, GER/GEMV and complex LACGV use RHS-row stride LDB and advance
    // through 1+NRHS*LDB, including for a single RHS.
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_lu_band_limits

#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_BAND_LIMITS_H_
