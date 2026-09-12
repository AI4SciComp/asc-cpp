#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_PIVOTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_PIVOTS_H_

#include <cstddef>
#include <span>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"

namespace asc::internal_indefinite_rook {

// A negative pair describes two ordered interchanges, one per entry. Bounds
// are the complete active principal block for both targets. Search-path
// restrictions are deliberately not inferred from values; common-operation
// provenance remains a precondition of the nominal rook factor view.
template <typename Integer>
Status Paired(std::span<const Integer> values, extent_t order,
              DenseBlasTriangle triangle) {
  const bool upper = triangle == DenseBlasTriangle::kUpper;
  if ((!upper && triangle != DenseBlasTriangle::kLower) || order < 0 ||
      values.size() != static_cast<std::size_t>(order)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  for (extent_t i = 0; i < order;) {
    const index_t p = values[static_cast<std::size_t>(i)];
    if (p == 0 || p < -order || p > order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (p > 0) {
      if ((upper && p > i + 1) || (!upper && p < i + 1)) {
        return Status(ErrorCode::kInvalidArgument);
      }
      ++i;
      continue;
    }
    if (i + 1 >= order) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const index_t q = values[static_cast<std::size_t>(i + 1)];
    if (q >= 0 || q < -order || (upper && (-p > i + 2 || -q > i + 2)) ||
        (!upper && (-p < i + 1 || -q < i + 1))) {
      return Status(ErrorCode::kInvalidArgument);
    }
    i += 2;
  }
  return Status::Ok();
}

}  // namespace asc::internal_indefinite_rook

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_ROOK_PIVOTS_H_
