#ifndef ASC_DENSE_LAPACK_INTERNAL_SCALAR_SORT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_SCALAR_SORT_COUNTS_H_
#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_tridiagonal_counts.h"
namespace asc::internal_scalar_sort_counts {
// START+ENDD<=2*N, ENDD+1 and insertion-loop terminal N+1 fit the actual
// integer. The source documents its 32-slot stack domain as about2^32.
// Processing the smaller partition first halves active lengths; with insertion
// leaves of size<=21, N<=2^32 needs at most29 simultaneously pending entries.
inline Status Count(extent_t n, extent_t limit) {
  if (!internal_tridiagonal_counts::Supported(limit) || n < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  constexpr extent_t kStackLimit = extent_t{1} << 32;
  return n > std::min(limit / 2, kStackLimit) ? Status(ErrorCode::kOverflow)
                                              : Status::Ok();
}
}  // namespace asc::internal_scalar_sort_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_SCALAR_SORT_COUNTS_H_
