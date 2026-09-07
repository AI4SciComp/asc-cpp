#ifndef ASC_DENSE_LAPACK_INTERNAL_LU_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_LU_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

// Private arithmetic for the pinned Reference-LAPACK 3.12.1 source routes.
// Boundary tests use integers, never unbacked matrix descriptors.
namespace asc::internal_lapack_lu {

enum class FactorRoute : std::uint8_t { kBlocked, kRecursive, kUnblocked };

inline bool SupportedLimit(extent_t limit) {
  return limit == std::numeric_limits<std::int32_t>::max() ||
         limit == std::numeric_limits<std::int64_t>::max();
}

inline Status CheckFactor(FactorRoute route, extent_t rows, extent_t columns,
                          extent_t leading, extent_t limit) {
  if (!SupportedLimit(limit) ||
      (route != FactorRoute::kBlocked && route != FactorRoute::kRecursive &&
       route != FactorRoute::kUnblocked)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rows < 0 || columns < 0 || rows > limit || columns > limit ||
      leading < std::max<extent_t>(1, rows) || leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  // All three quick-return with an empty dimension. Their one-row paths
  // inspect just the first pivot; neither swaps nor trailing updates run.
  if (rows <= 1 || columns == 0) {
    return Status::Ok();
  }
  // IxAMAX's unit-stride DO I=2,M still increments I after the last body.
  if (rows == limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (route == FactorRoute::kUnblocked) {
    // xGETF2 can call xSWAP(N,...,LDA): the final IX/IY is 1+N*LDA.
    // This also bounds xGER[U]'s 1+(N-J)*LDA cursor. LDA is the foreign
    // column stride, not an ASC row-major stride before explicit packing.
    if (columns > (limit - 1) / leading) {
      return Status(ErrorCode::kOverflow);
    }
  } else if (route == FactorRoute::kBlocked) {
    // ILAENV GE/TRF chooses NB=64. The blocked DO starts at 1; its exact
    // terminal value is 1+ceil(MIN(M,N)/64)*64. Small problems use GETRF2.
    const auto pivots = std::min(rows, columns);
    if (pivots > 64 && pivots > ((limit - 1) / 64) * 64) {
      return Status(ErrorCode::kOverflow);
    }
  }
  // GETRF2's recursive LASWP/TRSM/GEMM widths are N-N1 < N. LASWP
  // uses A(I,K), not a provider-INTEGER vector cursor multiplied by LDA.
  return Status::Ok();
}

inline Status CheckSolve(extent_t order, extent_t rhs_columns, extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order < 0 || rhs_columns < 0 || order > limit || rhs_columns > limit) {
    return Status(ErrorCode::kOverflow);
  }
  // GETRS returns before LASWP/TRSM when either dimension is empty.
  if (order != 0 && rhs_columns != 0 &&
      (order == limit || rhs_columns == limit)) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

// GETRI evaluates MAX(1,N*64) before checking LQUERY. S/C then calls
// SROUNDUP_LWORK, including an INTEGER(REAL(...)) conversion and a possible
// upward epsilon multiplication. Guard both before entering Fortran. D/Z
// directly converts to double; retain the exact raw count if it rounds down.
template <typename Real>
  requires(std::same_as<Real, float> || std::same_as<Real, double>)
Result<extent_t> InversePreferred(extent_t order, extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (order < 0 || order > limit / 64) {
    return Status(ErrorCode::kOverflow);
  }
  const auto raw = std::max<extent_t>(1, order * 64);
  const int exponent =
      limit == std::numeric_limits<std::int32_t>::max() ? 31 : 63;
  const Real exclusive_bound = std::ldexp(Real{1}, exponent);
  Real converted = static_cast<Real>(raw);
  if (converted >= exclusive_bound) {
    return Status(ErrorCode::kOverflow);
  }
  if constexpr (std::same_as<Real, float>) {
    if (static_cast<extent_t>(converted) < raw) {
      converted *= Real{1} + std::numeric_limits<Real>::epsilon();
      if (converted >= exclusive_bound) {
        return Status(ErrorCode::kOverflow);
      }
    }
  }
  return std::max(raw, static_cast<extent_t>(converted));
}

}  // namespace asc::internal_lapack_lu

#endif  // ASC_DENSE_LAPACK_INTERNAL_LU_COUNTS_H_
