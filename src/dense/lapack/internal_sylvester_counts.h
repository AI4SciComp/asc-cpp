#ifndef ASC_DENSE_LAPACK_INTERNAL_SYLVESTER_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_SYLVESTER_COUNTS_H_

#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_lapack_sylvester {

// Checked against the actual provider INTEGER limit, not a test-only backing
// descriptor. TRSYL evaluates M*N before converting to REAL; forward block
// loops and MIN(K+1,M) require representable successors. Reference DDOT and
// CDOTU/CDOTC advance strided row cursors once beyond the final accessed entry.
inline Status CheckDimensions(extent_t m, extent_t n, extent_t ldc,
                              bool transpose_a, bool transpose_b,
                              extent_t limit) {
  if (m < 0 || n < 0 || ldc < 1 || limit < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m > limit || n > limit || ldc > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (m == 0 || n == 0) {
    return Status::Ok();
  }
  if (ldc < m) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m == limit || n == limit || m > limit / n) {
    return Status(ErrorCode::kOverflow);
  }
  const auto cursor_fits = [limit](extent_t length, extent_t stride) {
    return length <= (limit - 1) / stride;
  };
  if ((!transpose_a && !cursor_fits(m - 1, m)) ||
      (transpose_b && !cursor_fits(n - 1, n)) || !cursor_fits(n - 1, ldc)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

// A/B are full caller-owned packed Schur matrices in both layouts because
// xLANGE('M') reads their entire logical squares. This count is ASC storage,
// not foreign INTEGER arithmetic or a fictional LWORK argument.
inline Result<extent_t> PackingEntries(extent_t m, extent_t n, bool row_c,
                                       std::size_t entry_bytes) {
  if (m < 0 || n < 0 || entry_bytes == 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m == 0 || n == 0) {
    return extent_t{0};
  }
  constexpr extent_t kLimit = std::numeric_limits<extent_t>::max();
  if (m > kLimit / m || n > kLimit / n) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t a = m * m;
  const extent_t b = n * n;
  if (b > kLimit - a) {
    return Status(ErrorCode::kOverflow);
  }
  extent_t total = a + b;
  if (row_c) {
    if (m > (kLimit - total) / n) {
      return Status(ErrorCode::kOverflow);
    }
    total += m * n;
  }
  if (static_cast<std::uint64_t>(total) >
      std::numeric_limits<std::size_t>::max() / entry_bytes) {
    return Status(ErrorCode::kOverflow);
  }
  return total;
}

}  // namespace asc::internal_lapack_sylvester

#endif  // ASC_DENSE_LAPACK_INTERNAL_SYLVESTER_COUNTS_H_
