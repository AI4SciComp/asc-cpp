#ifndef ASC_DENSE_LAPACK_INTERNAL_VECTOR_CONJUGATION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_VECTOR_CONJUGATION_COUNTS_H_
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_tridiagonal_counts.h"

namespace asc::internal_vector_conjugation_counts {
// Source LACGV loops end with I=N+1. Positive nonunit spacing also computes
// IOFF=1+N*INCX after the last access. Negative spacing starts at
// 1+(N-1)*abs(INCX) and finishes at1+INCX. Zero spacing repeats one entry.
// N<=1 uses no original increment in the foreign call; N=0 stays local.
inline Result<extent_t> Storage(extent_t n, stride_t increment,
                                extent_t limit) {
  if (!internal_tridiagonal_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n >= limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n <= 1) {
    return n;
  }
  if (increment < -limit || increment > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (increment > 1 && increment > (limit - 1) / n) {
    return Status(ErrorCode::kOverflow);
  }
  const stride_t magnitude = increment < 0 ? -increment : increment;
  if (magnitude > (limit - 1) / (n - 1)) {
    return Status(ErrorCode::kOverflow);
  }
  return 1 + (n - 1) * magnitude;
}
}  // namespace asc::internal_vector_conjugation_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_VECTOR_CONJUGATION_COUNTS_H_
