#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_SOLVE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_SOLVE_COUNTS_H_
#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
#include "internal_lu_band_limits.h"
namespace asc::internal_indefinite_aasen_two_stage_solve_counts {
inline Status Solve(extent_t n, extent_t nrhs, extent_t lda, extent_t ltb,
                    extent_t ldb, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0 || nrhs < 0 ||
      lda < 1 || ldb < 1 || ltb < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // Native LTB validation forms 4*N before the empty quick return. This
  // also bounds 2*N native pivot lifetimes. Original strides are checked
  // independently by the caller before any packing.
  if (n > limit / 4 || nrhs > limit || lda > limit || ldb > limit ||
      ltb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (ltb < 4 * n) {
    return Status(ErrorCode::kShape);
  }
  if (n == 0 || nrhs == 0) {
    // Checked empty calls do not enter the provider. Leading(empty RHS)
    // is one, even when its logical row count is greater than one.
    return Status::Ok();
  }
  if (lda < n || ldb < n) {
    return Status(ErrorCode::kShape);
  }
  const extent_t ldtb = ltb / n;
  const extent_t nb = std::min<extent_t>(192, (ldtb - 1) / 3);
  // GBTRS bounds nonunit RHS-row cursors and the terminal N/NRHS loops.
  // LASWP forward/reverse pivot cursors fit since N<=limit/4. Its
  // N32=(NRHS/32)*32 is <=limit-31 for either admitted signed INTEGER;
  // J's terminal 1+N32 and inner J+31 therefore also fit. The scalar
  // tail's NRHS+1 is covered by GBTRS. TRSM has N-NB<=N-1 rows.
  return internal_lu_band_limits::Solve(n, nb, nb, ldtb, nrhs, ldb, limit);
}
}  // namespace asc::internal_indefinite_aasen_two_stage_solve_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_SOLVE_COUNTS_H_
