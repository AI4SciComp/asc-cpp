#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_SOLVE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_SOLVE_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_packed_cholesky_solve_counts {

// PPTRS calls both transpose/conjugate and non-transpose TPSV with INCX=1.
// In either triangle one of those paths evaluates N*(N+1) before division.
// This bound also covers every p+1 packed cursor and N+1 final loop control.
// NRHS+1 is the PPTRS outer DO-loop terminal value. Empty operations complete
// locally before narrowing any metadata, reading factors or packing RHS.
inline Status Solve(extent_t n, extent_t nrhs, extent_t ldb, extent_t limit) {
  if (n < 0 || nrhs < 0 || ldb < 1 || ldb < n || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0 || nrhs == 0) {
    return Status::Ok();
  }
  if (n >= limit || nrhs >= limit || ldb > limit || n > limit / (n + 1)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_packed_cholesky_solve_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_SOLVE_COUNTS_H_
