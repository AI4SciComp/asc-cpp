#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_HELPERS_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_HELPERS_H_

#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_lu_helpers {

// Pure integer metadata only; extreme tests need no invented matrix backing.
// Capacity is the one-based largest used IPIV position, not the swap count.
inline Result<extent_t> SwapCapacity(extent_t rows, extent_t columns,
                                     extent_t leading_dimension,
                                     index_t first_row, extent_t row_count,
                                     index_t increment, extent_t integer_max) {
  if (rows < 0 || columns < 0 || leading_dimension < 1 || first_row < 0 ||
      row_count < 0 || integer_max < 1 || first_row > rows ||
      row_count > rows - first_row) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rows > integer_max || columns > integer_max ||
      leading_dimension > integer_max || increment > integer_max ||
      increment < -integer_max - 1) {
    return Status(ErrorCode::kOverflow);
  }
  if (columns == 0 || row_count == 0 || increment == 0) {
    return extent_t{0};
  }
  // The final tail-column DO update forms N+1. Forward row DO forms K2+1.
  if (columns == integer_max ||
      (increment > 0 && first_row + row_count == integer_max)) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t k1 = first_row + 1;
  if (increment > 0) {
    // IX is updated once after the last pivot read, too.
    if (increment > (integer_max - k1) / row_count) {
      return Status(ErrorCode::kOverflow);
    }
    return k1 + (row_count - 1) * increment;
  }
  // Unsigned magnitude also represents abs(INT64_MIN). A single reverse
  // swap permits that increment: initial product is zero and final IX fits.
  const auto magnitude =
      std::uint64_t{0} - static_cast<std::uint64_t>(increment);
  if (static_cast<std::uint64_t>(row_count - 1) >
      static_cast<std::uint64_t>(integer_max - k1) / magnitude) {
    return Status(ErrorCode::kOverflow);
  }
  return k1 + static_cast<extent_t>(static_cast<std::uint64_t>(row_count - 1) *
                                    magnitude);
}

inline Status ScaleDimensions(extent_t rows, extent_t columns,
                              extent_t leading_dimension, bool scales,
                              extent_t integer_max) {
  if (rows < 0 || columns < 0 || leading_dimension < 1 || integer_max < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rows > integer_max || columns > integer_max ||
      leading_dimension > integer_max) {
    return Status(ErrorCode::kOverflow);
  }
  if (scales && rows != 0 && columns != 0 &&
      (rows == integer_max || columns == integer_max)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_lu_helpers

#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_HELPERS_H_
