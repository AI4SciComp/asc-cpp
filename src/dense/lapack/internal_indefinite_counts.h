#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_indefinite_counts {

inline bool Supported(extent_t limit) {
  return limit == std::numeric_limits<std::int32_t>::max() ||
         limit == std::numeric_limits<std::int64_t>::max();
}

// Positive-increment BLAS cursors finish at 1+count*increment. This is
// different from the last array subscript and must fit provider INTEGER.
inline Status Cursor(extent_t count, extent_t increment, extent_t limit) {
  if (!Supported(limit) || count < 0 || increment < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return count > (limit - 1) / increment ? Status(ErrorCode::kOverflow)
                                         : Status::Ok();
}

// The unblocked pivot search's longest strided IAMAX vector has N-1 entries;
// IAMAX returns before its cursor arithmetic for vector length one. All
// unblocked strided swaps have at most N-2 entries. N+1 loop endpoints remain
// independently checked. Blocked calls add strided COPY paths, but N>64 there.
inline Status Factor(extent_t order, extent_t leading, extent_t limit) {
  if (!Supported(limit) || order < 0 || leading < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order >= limit || leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  return order > 2 ? Cursor(order - 1, leading, limit) : Status::Ok();
}

// SYTRS/HETRS use RHS rows as strided BLAS vectors: SWAP, SCAL, GEMV,
// GER and (Hermitian) LACGV. Empty wrappers never enter their foreign loops.
inline Status Solve(extent_t order, extent_t rhs_columns, extent_t leading,
                    extent_t limit) {
  if (!Supported(limit) || order < 0 || rhs_columns < 0 || leading < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order > limit || rhs_columns > limit || leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0 || rhs_columns == 0) {
    return Status::Ok();
  }
  if (order == limit) {
    return Status(ErrorCode::kOverflow);
  }
  return Cursor(rhs_columns, leading, limit);
}

// The pinned classic SYTRF/HETRF ILAENV(ISPEC=1) branch returns NB=64,
// independently of available LWORK. Even a minimum-work execution evaluates
// 64*N and writes its rounded preferred value. S/C use SROUNDUP_LWORK;
// D/Z assign INTEGER directly to the real WORK component.
template <typename Real>
Result<extent_t> Preferred(extent_t order, extent_t limit) {
  static_assert(std::is_same_v<Real, float> || std::is_same_v<Real, double>);
  if (!Supported(limit) || order < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order > limit / 64) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t raw = std::max<extent_t>(1, 64 * order);
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

}  // namespace asc::internal_indefinite_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_COUNTS_H_
