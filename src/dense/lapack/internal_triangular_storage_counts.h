#ifndef ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_STORAGE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_STORAGE_COUNTS_H_

#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_triangular_storage_counts {
struct Counts {
  extent_t dense;
  extent_t packed;
};

// Pure arithmetic for the source INTEGER loops, with no fabricated arrays.
inline Result<Counts> Count(extent_t n, extent_t leading, bool to_packed,
                            extent_t limit) {
  if (n < 0 || leading < 1 || leading < n || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    return Counts{0, 0};
  }
  // I/J form N+1. K is incremented up to N*(N+1)/2. The public packed
  // descriptor also requires its undivided product to fit extent_t. That
  // stronger host bound protects all dense and row-packed offset formulas.
  if (n >= limit || leading > limit ||
      n > std::numeric_limits<extent_t>::max() / (n + 1)) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t left = n % 2 == 0 ? n / 2 : n;
  const extent_t right = n % 2 == 0 ? n + 1 : (n + 1) / 2;
  if (left > limit / right) {
    return Status(ErrorCode::kOverflow);
  }
  // The existing neutral workspace validator bounds kScratch entries by the
  // LP64 INTEGER maximum. Dense-output staging must satisfy that ASC rule even
  // though the foreign routine has no LWORK argument. kLayout is exempt.
  if (!to_packed && n * n > limit) {
    return Status(ErrorCode::kOverflow);
  }
  // Actual reachable descriptor spans and aggregate workspace bytes are
  // validated separately. Prepared providers use address-sized array offsets.
  return Counts{n * n, left * right};
}
}  // namespace asc::internal_triangular_storage_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_TRIANGULAR_STORAGE_COUNTS_H_
