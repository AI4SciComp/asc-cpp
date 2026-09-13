#ifndef ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_LIMITS_H_
#define ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_LIMITS_H_

#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc::internal_cholesky_limits {

// A pure integer predicate: tests can exercise extreme bounds without forging
// backing spans. Includes the final Fortran DO-variable/strided-index update.
inline Status CheckOrder(extent_t order, extent_t leading_dimension,
                         extent_t integer_max, extent_t loop_increment,
                         bool has_strided_vectors) {
  if (order < 0 || leading_dimension < 1 || integer_max < 1 ||
      loop_increment < 1 || loop_increment > integer_max) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order > integer_max - loop_increment || leading_dimension > integer_max) {
    return Status(ErrorCode::kOverflow);
  }
  // POTF2 and POTRI->LAUU2 have INTEGER vector cursors with positive INC=LDA.
  // The bound covers (length-1)*INC+1 and the final cursor increment. This is
  // provider index arithmetic, not the independently ASC-sized packing region.
  if (has_strided_vectors && order != 0 &&
      leading_dimension > integer_max / order) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

inline Status CheckRightHandSides(extent_t order, extent_t right_hand_sides,
                                  extent_t integer_max) {
  if (order < 0 || right_hand_sides < 0 || integer_max < 1) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (right_hand_sides > integer_max ||
      (order != 0 && right_hand_sides == integer_max)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

}  // namespace asc::internal_cholesky_limits

#endif  // ASC_DENSE_LAPACK_INTERNAL_CHOLESKY_LIMITS_H_
