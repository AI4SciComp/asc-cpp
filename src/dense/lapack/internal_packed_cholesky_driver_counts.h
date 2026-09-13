#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_DRIVER_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_DRIVER_COUNTS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_cholesky_counts.h"
#include "internal_packed_cholesky_solve_counts.h"

namespace asc::internal_packed_cholesky_driver_counts {

// PPSV validates its own arguments and always invokes PPTRF, even for NRHS=0.
// PPTRS runs only after successful factorization. Its NRHS=0 return needs no
// TPSV cursor bound; positive NRHS admits both selected TPSV directions and
// the NRHS+1 final loop value. Only N=0 is an ASC-local completion without
// narrowing. The actual LDB must fit whenever the foreign driver is entered.
inline Status Driver(extent_t n, extent_t nrhs, extent_t ldb,
                     DenseBlasTriangle triangle, extent_t limit) {
  if (n < 0 || nrhs < 0 || ldb < 1 || ldb < n || limit < 3 ||
      (triangle != DenseBlasTriangle::kUpper &&
       triangle != DenseBlasTriangle::kLower)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Status::Ok();
  }
  if (nrhs > limit || ldb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  Status status = internal_packed_cholesky_counts::Factor(n, triangle, limit);
  if (!status.ok() || nrhs == 0) {
    return status;
  }
  return internal_packed_cholesky_solve_counts::Solve(n, nrhs, ldb, limit);
}

}  // namespace asc::internal_packed_cholesky_driver_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_CHOLESKY_DRIVER_COUNTS_H_
