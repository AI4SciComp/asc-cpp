#ifndef ASC_DENSE_LAPACK_INTERNAL_MIXED_POSITIVE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_MIXED_POSITIVE_COUNTS_H_
#include <algorithm>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_cholesky_limits.h"

namespace asc::internal_mixed_positive_counts {
struct Counts {
  extent_t lower = 0;
  extent_t residual = 0;
  extent_t solution = 0;
};
// Both drivers form PTSX=1+N*N in native INTEGER. POTRF uses the
// pinned ILAENV block size 64; conversions/norms have N+1/NRHS+1 terminals.
inline Result<Counts> Query(extent_t n, extent_t nrhs, extent_t lda,
                            extent_t limit, bool complex) {
  const Status factor =
      internal_cholesky_limits::CheckOrder(n, lda, limit, 64, false);
  if (!factor.ok()) {
    return factor;
  }
  const Status solve =
      internal_cholesky_limits::CheckRightHandSides(n, nrhs, limit);
  if (!solve.ok()) {
    return solve;
  }
  if (n == 0) {
    return Counts{};
  }
  if (n > (limit - 1) / n || nrhs > (limit - n * n) / n) {
    return Status(ErrorCode::kOverflow);
  }
  const auto solution = n * nrhs;
  // With zero RHS the Fortran call still forms SWORK(PTS X). One additional
  // live lower scalar keeps this otherwise inactive actual argument in bounds.
  const auto lower = n * n + std::max<extent_t>(1, solution);
  // DSPOSV passes WORK to DLANSY('I') before the zero-RHS loops: N entries
  // are active even though the prose WORK dimension says N*NRHS.
  return Counts{lower, complex ? solution : std::max(n, solution), solution};
}
}  // namespace asc::internal_mixed_positive_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_MIXED_POSITIVE_COUNTS_H_
