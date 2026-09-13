#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_SOLVE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_SOLVE_COUNTS_H_
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_aasen_solve_counts {
inline Status Solve(extent_t n, extent_t nrhs, extent_t lda, extent_t ldb,
                    extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0 || nrhs < 0 ||
      lda < 1 || ldb < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n > limit || nrhs > limit || lda > limit || ldb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  // The source evaluates 3*N before subtracting two, passes LDA+1 to LACPY,
  // and GTSV advances its RHS loop beyond NRHS. SWAP is skipped for N=1.
  if (n > limit / 3 || lda == limit || nrhs == limit) {
    return Status(ErrorCode::kOverflow);
  }
  return n > 1 ? internal_indefinite_counts::Cursor(nrhs, ldb, limit)
               : Status::Ok();
}
template <typename Real>
Result<extent_t> Preferred(extent_t n, extent_t limit) {
  static_assert(std::is_same_v<Real, float> || std::is_same_v<Real, double>);
  if (!internal_indefinite_counts::Supported(limit) || n < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n > limit / 3) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t minimum = 3 * n - 2;
  Real rounded = static_cast<Real>(minimum);
  const Real upper = std::ldexp(Real{1}, limit == INT32_MAX ? 31 : 63);
  if (!(rounded < upper)) {
    return Status(ErrorCode::kOverflow);
  }
  if constexpr (std::is_same_v<Real, float>) {
    if (static_cast<extent_t>(rounded) < minimum) {
      rounded *= Real{1} + std::numeric_limits<Real>::epsilon();
      if (!(rounded < upper)) {
        return Status(ErrorCode::kOverflow);
      }
    }
  }
  // D/Z native queries may round down; the integer ASC plan retains the
  // exact source minimum independently of that scalar conversion.
  return std::max(minimum, static_cast<extent_t>(rounded));
}
}  // namespace asc::internal_indefinite_aasen_solve_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_SOLVE_COUNTS_H_
