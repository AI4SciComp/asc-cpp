#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_EXPERT_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_counts.h"

namespace asc::internal_indefinite_expert_counts {

// CON/RFS/VX enter LACN2, whose final estimator expression evaluates 3*N
// in provider INTEGER even on the complex 2*N scalar-workspace route.
// The bound also covers N+1, 2*N+1 and unit-increment terminal cursors.
inline Status Estimator(extent_t order, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || order < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  return order > limit / 3 ? Status(ErrorCode::kOverflow) : Status::Ok();
}

// RFS loops over right-hand sides, including the terminal NRHS+1 value.
// Its internal TRS calls always have one compact RHS, independent of the
// caller's original B/X strides. VX separately checks its initial TRS call.
inline Status Refinement(extent_t order, extent_t rhs_columns, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || order < 0 ||
      rhs_columns < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rhs_columns > limit || order > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (order == 0 || rhs_columns == 0) {
    return Status::Ok();
  }
  if (rhs_columns == limit) {
    return Status(ErrorCode::kOverflow);
  }
  return Estimator(order, limit);
}

// This is the exact SROUNDUP_LWORK conversion, or the D/Z returned REAL
// conversion, for an already source-checked integer expression. Retain raw
// when a double rounds downward. Reject both the initial and epsilon-rounded
// single-precision upper-exclusive provider INTEGER boundary before casting.
template <typename Real>
Result<extent_t> Rounded(extent_t raw, extent_t limit) {
  static_assert(std::is_same_v<Real, float> || std::is_same_v<Real, double>);
  if (!internal_indefinite_counts::Supported(limit) || raw < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (raw > limit) {
    return Status(ErrorCode::kOverflow);
  }
  Real returned = static_cast<Real>(raw);
  const Real upper = std::ldexp(Real{1}, limit == INT32_MAX ? 31 : 63);
  if (!(returned < upper)) {
    return Status(ErrorCode::kOverflow);
  }
  if constexpr (std::is_same_v<Real, float>) {
    if (static_cast<extent_t>(returned) < raw) {
      returned *= Real{1} + std::numeric_limits<Real>::epsilon();
      if (!(returned < upper)) {
        return Status(ErrorCode::kOverflow);
      }
    }
  }
  return std::max(raw, static_cast<extent_t>(returned));
}

// All SV routes evaluate the TRF 64*N requirement. SYSV then converts the
// TRF floating query to INTEGER and S/C round that integer a second time;
// the first checked rounded value is already exactly representable. HESV
// directly computes 64*N. These return the same bounded capacity here.
template <typename Real>
Result<extent_t> Driver(extent_t order, extent_t limit) {
  return internal_indefinite_counts::Preferred<Real>(order, limit);
}

// VX evaluates MAX(1,3*N) for real or MAX(1,2*N) for complex before its
// parameter checks. FACT=N adds 64*N; FACT=F does not impose that bound.
template <typename Real>
Result<extent_t> Expert(extent_t order, bool complex, bool factor,
                        extent_t limit) {
  Status estimator = Estimator(order, limit);
  if (!estimator.ok()) {
    return estimator;
  }
  const extent_t minimum = std::max<extent_t>(1, (complex ? 2 : 3) * order);
  if (!factor) {
    return Rounded<Real>(minimum, limit);
  }
  if (order > limit / 64) {
    return Status(ErrorCode::kOverflow);
  }
  return Rounded<Real>(std::max(minimum, 64 * order), limit);
}

}  // namespace asc::internal_indefinite_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_EXPERT_COUNTS_H_
