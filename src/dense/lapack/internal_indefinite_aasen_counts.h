#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_COUNTS_H_
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_aasen_counts {
// The pinned single-stage ILAENV branch uses NB=64. Even a minimum-work
// call computes (NB+1)*N and WORK(N*NB+1). Its initial strided COPY has N
// entries, so its terminal cursor is 1+N*LDA, unlike classic/RK TRF.
inline Status Factor(extent_t n, extent_t lda, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0 || lda < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n >= limit || lda > limit) {
    return Status(ErrorCode::kOverflow);
  }
  return n > 1 ? internal_indefinite_counts::Cursor(n, lda, limit)
               : Status::Ok();
}
template <typename Real>
Result<extent_t> Preferred(extent_t n, bool complex_symmetric, extent_t limit) {
  static_assert(std::is_same_v<Real, float> || std::is_same_v<Real, double>);
  if (!internal_indefinite_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n <= 1 && !complex_symmetric) {
    return extent_t{1};
  }
  if (n > limit / 65) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t raw = 65 * n;
  Real rounded = static_cast<Real>(raw);
  const Real upper = std::ldexp(Real{1}, limit == INT32_MAX ? 31 : 63);
  if (!(rounded < upper)) {
    return Status(ErrorCode::kOverflow);
  }
  if constexpr (std::is_same_v<Real, float>) {
    if (static_cast<extent_t>(rounded) < raw) {
      rounded *= Real{1} + std::numeric_limits<Real>::epsilon();
      if (!(rounded < upper)) {
        return Status(ErrorCode::kOverflow);
      }
    }
  }
  return std::max(raw, static_cast<extent_t>(rounded));
}
}  // namespace asc::internal_indefinite_aasen_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_COUNTS_H_
