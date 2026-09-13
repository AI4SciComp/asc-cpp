#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_BLOCK_INVERSE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_BLOCK_INVERSE_COUNTS_H_
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_block_inverse_counts {
struct Options {
  bool hermitian;
  bool explicit_block;
  extent_t block_size;
};
// TRI2X does not validate NB; zero would prevent loop progress. Its WORK
// dimensions and block endpoints must fit INTEGER, but it has no INTEGER
// LWORK product. TRI2 does have that additional product constraint.
inline Result<extent_t> Workspace(extent_t order, extent_t leading,
                                  Options options, bool upper, extent_t limit) {
  namespace counts = internal_indefinite_counts;
  const extent_t nb = options.block_size;
  if (!counts::Supported(limit) || order < 0 || leading < 1 || nb < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order > limit || leading > limit || nb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0) {
    return extent_t{0};
  }
  if (nb > limit - 3 || order > limit - nb - 1) {
    return Status(ErrorCode::kOverflow);
  }
  const bool blocked = options.explicit_block || order > nb;
  if (order > 2 && (!options.hermitian || (blocked && !upper))) {
    const Status status = counts::Cursor(order - 2, leading, limit);
    if (!status.ok()) {
      return status;
    }
  }
  // The nested upper TRTRI's DO J=1,N,64 advances once past its last block.
  if (blocked && upper && order > 64 &&
      (order - 1) / 64 + 1 > (limit - 1) / 64) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t rows = order + nb + 1;
  const extent_t columns = nb + 3;
  const extent_t cap =
      options.explicit_block ? std::numeric_limits<extent_t>::max() : limit;
  if (rows > cap / columns) {
    return Status(ErrorCode::kOverflow);
  }
  return rows * columns;
}
}  // namespace asc::internal_indefinite_block_inverse_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_BLOCK_INVERSE_COUNTS_H_
