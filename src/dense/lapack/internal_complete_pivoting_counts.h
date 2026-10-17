#ifndef ASC_DENSE_LAPACK_INTERNAL_COMPLETE_PIVOTING_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_COMPLETE_PIVOTING_COUNTS_H_

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_complete_pivoting_counts {
struct Counts {
  extent_t scalar = 0;
  extent_t integer = 0;
};

// The actual compact LDA=N reaches N*N+1 in BLAS swap cursors. Explicit
// scalar staging is N*N (+N for the solve RHS), and the two actual-width
// pivot arrays share 2*N native integer entries. Check before multiplication;
// the generic workspace byte/layout validator remains separately required.
inline Result<Counts> Count(extent_t n, bool solve, extent_t limit) {
  if (n < 0 || limit <= 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Counts{};
  }
  if (n > limit / 2 || n > (limit - 1) / n) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t matrix = n * n;
  if (solve && matrix > limit - n) {
    return Status(ErrorCode::kOverflow);
  }
  return Counts{matrix + (solve ? n : 0), 2 * n};
}
}  // namespace asc::internal_complete_pivoting_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_COMPLETE_PIVOTING_COUNTS_H_
