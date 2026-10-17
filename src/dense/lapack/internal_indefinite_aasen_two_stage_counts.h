#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
#include "internal_lu_band_limits.h"
#include "internal_lu_counts.h"

namespace asc::internal_indefinite_aasen_two_stage_counts {

// The pinned ILAENV recognizes character 11 of *_AA_2STAGE and selects 192.
// TB is a persistent output, so its entire supplied length is the native LTB.
inline Status Factor(extent_t n, extent_t lda, extent_t ltb, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0 || lda < 1 ||
      ltb < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n > limit / 192 || lda > limit || ltb >= limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (ltb < 4 * n || lda < n) {
    return Status(ErrorCode::kShape);
  }
  const extent_t ldtb = ltb / n;
  const extent_t nb = std::min<extent_t>(192, (ldtb - 1) / 3);
  // Upper COPY and outer-factor SWAP/LACGV use at most N-1 entries at LDA.
  // WORK's explicit (K-1)*N offsets and NT=(N+NB-1)/NB fit under 192*N.
  if (n > 1) {
    auto status = internal_indefinite_counts::Cursor(n - 1, lda, limit);
    if (!status.ok()) {
      return status;
    }
  }
  // The largest legal NB bounds all band-LU strided vectors, including the
  // <=NB vectors in SYGST/HEGST. Actual workspace can only reduce NB.
  auto status = internal_lu_band_limits::Factor(n, n, nb, nb, ldtb, limit);
  if (!status.ok()) {
    return status;
  }
  // Panel GETRF uses A/LDA below the diagonal or a copy in WORK with LD=N.
  return internal_lapack_lu::CheckFactor(
      internal_lapack_lu::FactorRoute::kBlocked, n - 1, nb, lda, limit);
}

template <typename Real>
Result<extent_t> Preferred(extent_t n, extent_t limit) {
  static_assert(std::is_same_v<Real, float> || std::is_same_v<Real, double>);
  if (!internal_indefinite_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n > limit / 192) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t raw = 192 * n;
  Real rounded = static_cast<Real>(raw);
  const Real bound = std::ldexp(Real{1}, limit == INT32_MAX ? 31 : 63);
  if (!(rounded < bound)) {
    return Status(ErrorCode::kOverflow);
  }
  if constexpr (std::is_same_v<Real, float>) {
    if (static_cast<extent_t>(rounded) < raw) {
      rounded *= Real{1} + std::numeric_limits<Real>::epsilon();
      if (!(rounded < bound)) {
        return Status(ErrorCode::kOverflow);
      }
    }
  }
  return std::max(raw, static_cast<extent_t>(rounded));
}

inline extent_t BlockWidth(extent_t n, extent_t ltb, extent_t lwork) {
  return std::min<extent_t>({192, (ltb / n - 1) / 3, lwork / n});
}

}  // namespace asc::internal_indefinite_aasen_two_stage_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_COUNTS_H_
