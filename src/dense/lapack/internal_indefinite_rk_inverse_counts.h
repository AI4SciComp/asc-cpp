#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_INVERSE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_INVERSE_COUNTS_H_
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_rk_inverse_counts {
struct Options {
  bool hermitian;
  bool explicit_block;
  extent_t block_size;
};
// TRI_3X does not validate NB; zero would prevent loop progress. Its WORK
// dimensions and block endpoints must fit INTEGER, but it has no INTEGER
// LWORK product. TRI_3 does have that additional product constraint.
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
  if (order > 2 && (!options.hermitian || !upper)) {
    const Status status = counts::Cursor(order - 2, leading, limit);
    if (!status.ok()) {
      return status;
    }
  }
  // The nested upper TRTRI's DO J=1,N,64 advances once past its last block.
  if (upper && order > 64 && (order - 1) / 64 + 1 > (limit - 1) / 64) {
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
// Mirror only the driver's scalar WORK result representation. Guard conversion
// before SROUNDUP_LWORK's native INT and any local INTEGER conversion.
template <typename Real>
Result<extent_t> Preferred(extent_t raw, extent_t limit) {
  static_assert(std::is_same_v<Real, float> || std::is_same_v<Real, double>);
  if (!internal_indefinite_counts::Supported(limit) || raw < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (raw > limit) {
    return Status(ErrorCode::kOverflow);
  }
  Real rounded = static_cast<Real>(raw);
  const Real upper = std::ldexp(Real{1}, limit == INT32_MAX ? 31 : 63);
  if (!(rounded < upper)) {
    return Status(ErrorCode::kOverflow);
  }
  if constexpr (std::is_same_v<Real, float>) {
    if (static_cast<extent_t>(rounded) < raw) {
      rounded *= Real{1} + std::numeric_limits<Real>::epsilon();
      if (!(rounded < upper)) {
        return Status(ErrorCode::kOverflow);
      }
    }
  }
  return std::max(raw, static_cast<extent_t>(rounded));
}
}  // namespace asc::internal_indefinite_rk_inverse_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_RK_INVERSE_COUNTS_H_
