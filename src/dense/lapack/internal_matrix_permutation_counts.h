#ifndef ASC_DENSE_LAPACK_INTERNAL_MATRIX_PERMUTATION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_MATRIX_PERMUTATION_COUNTS_H_

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_matrix_permutation_counts {
// Pure metadata arithmetic. Empty matrices never enter Fortran or scan K.
inline Result<extent_t> Count(extent_t m, extent_t n, extent_t limit) {
  if (m < 0 || n < 0 || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m == 0 || n == 0) {
    return extent_t{0};
  }
  // Source DO terminals can form M+1 and N+1. Native X is always compact
  // staged column storage with LDX=M. The neutral kScratch region additionally
  // bounds its entry count by the native INTEGER limit.
  if (m >= limit || n >= limit || m > limit / n) {
    return Status(ErrorCode::kOverflow);
  }
  return m * n;
}
}  // namespace asc::internal_matrix_permutation_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_MATRIX_PERMUTATION_COUNTS_H_
