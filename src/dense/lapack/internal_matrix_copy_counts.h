#ifndef ASC_DENSE_LAPACK_INTERNAL_MATRIX_COPY_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_MATRIX_COPY_COUNTS_H_
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
namespace asc::internal_matrix_copy_counts {
// Pure arithmetic: tests of foreign loop terminals never invent array storage.
inline Result<extent_t> Count(extent_t m, extent_t n, extent_t leading,
                              extent_t limit) {
  if (m < 0 || n < 0 || leading < 1 || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m == 0 || n == 0) {
    return extent_t{0};
  }
  // The selected pinned copy loops form the terminal index M+1/N+1. Linear
  // native subscripts use the compiler's address-sized arithmetic; the checked
  // descriptor has already bounded their complete actual reachable span.
  if (m >= limit || n >= limit || leading > limit ||
      n > std::numeric_limits<extent_t>::max() / m) {
    return Status(ErrorCode::kOverflow);
  }
  return m * n;
}
}  // namespace asc::internal_matrix_copy_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_MATRIX_COPY_COUNTS_H_
