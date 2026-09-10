#ifndef ASC_DENSE_LAPACK_INTERNAL_MIXED_GENERAL_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_MIXED_GENERAL_COUNTS_H_
#include <algorithm>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_lu_counts.h"

namespace asc::internal_mixed_general_counts {
struct Counts {
  extent_t lower = 0;
  extent_t residual = 0;
  extent_t solution = 0;
};
// The pinned drivers form PTSX=1+N*N in native INTEGER. Their conversion,
// norm and residual loops have N+1/NRHS+1 terminals. Reuse the admitted
// blocked GETRF and GETRS bounds; no hypothetical dense workspace is inferred.
inline Result<Counts> Query(extent_t n, extent_t nrhs, extent_t lda,
                            extent_t limit, bool complex) {
  const Status factor = internal_lapack_lu::CheckFactor(
      internal_lapack_lu::FactorRoute::kBlocked, n, n, lda, limit);
  if (!factor.ok()) {
    return factor;
  }
  const Status solve = internal_lapack_lu::CheckSolve(n, nrhs, limit);
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
  // DSGESV passes WORK to DLANGE('I') before the zero-RHS loops: N entries
  // are active even though the prose WORK dimension says N*NRHS.
  return Counts{lower, complex ? solution : std::max(n, solution), solution};
}
}  // namespace asc::internal_mixed_general_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_MIXED_GENERAL_COUNTS_H_
