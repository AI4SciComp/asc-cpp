#ifndef ASC_DENSE_LAPACK_INTERNAL_MATRIX_SCALE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_MATRIX_SCALE_COUNTS_H_

#include <algorithm>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_matrix_scale_counts {
struct Counts {
  extent_t leading;
  extent_t packing_entries;
};

// Pure arithmetic for actual LASCL loop terminals and band-index expressions.
// Tests at integer limits do not construct artificial matrix pointers.
inline Result<Counts> Count(char type, extent_t m, extent_t n, extent_t lower,
                            extent_t upper, extent_t leading, bool row_major,
                            extent_t limit) {
  const bool full = type == 'G' || type == 'L' || type == 'U' || type == 'H';
  const bool symmetric_band = type == 'B' || type == 'Q';
  if ((!full && !symmetric_band && type != 'Z') || m < 0 || n < 0 ||
      lower < 0 || upper < 0 || leading < 1 || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (!full && (lower > std::max<extent_t>(m - 1, 0) ||
                upper > std::max<extent_t>(n - 1, 0) ||
                (symmetric_band && (m != n || lower != upper)))) {
    return Status(ErrorCode::kShape);
  }
  extent_t stored_rows = m;
  if (symmetric_band) {
    const auto rows = CheckedAdd<extent_t>(lower, 1);
    if (!rows.ok()) {
      return rows.status();
    }
    stored_rows = *rows;
  } else if (type == 'Z') {
    const auto twice = CheckedMultiply<extent_t>(lower, 2);
    if (!twice.ok()) {
      return twice.status();
    }
    const auto bands = CheckedAdd<extent_t>(*twice, upper);
    if (!bands.ok()) {
      return bands.status();
    }
    const auto rows = CheckedAdd<extent_t>(*bands, 1);
    if (!rows.ok()) {
      return rows.status();
    }
    stored_rows = *rows;
  }
  if (m == 0 || n == 0) {
    return Counts{1, 0};
  }
  const extent_t native_leading = row_major ? stored_rows : leading;
  if (m >= limit || n >= limit || stored_rows >= limit ||
      native_leading < stored_rows || native_leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (type == 'Z') {
    // The native band path forms lower+upper+1+m before subtracting j.
    const auto bands = CheckedAdd<extent_t>(lower, upper);
    if (!bands.ok()) {
      return bands.status();
    }
    const auto diagonal = CheckedAdd<extent_t>(*bands, 1);
    if (!diagonal.ok() || *diagonal > limit - m) {
      return Status(ErrorCode::kOverflow);
    }
  }
  extent_t packing = 0;
  if (row_major) {
    if (stored_rows > std::numeric_limits<extent_t>::max() / n) {
      return Status(ErrorCode::kOverflow);
    }
    packing = stored_rows * n;
  }
  return Counts{native_leading, packing};
}
}  // namespace asc::internal_matrix_scale_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_MATRIX_SCALE_COUNTS_H_
