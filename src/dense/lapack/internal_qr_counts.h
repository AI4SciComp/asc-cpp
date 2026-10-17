#ifndef ASC_DENSE_LAPACK_INTERNAL_QR_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_QR_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

// Private source-pinned arithmetic, independently testable without constructing
// fake large arrays. No foreign invocation or allocation occurs here.
namespace asc::internal_lapack_qr {

enum class Operation : std::uint8_t { kGeqrf, kGeqr2, kGenerate, kApply };

struct Counts {
  extent_t minimum;
  extent_t raw_preferred;
};

// Bounds the actual nonnegative foreign integer expression, including the
// final addition. This is not a bound on ASC-only row strides or packing.
inline Result<extent_t> MultiplyAdd(extent_t value, extent_t multiplier,
                                    extent_t addition, extent_t limit) {
  if (value < 0 || multiplier < 0 || addition < 0 || limit < addition ||
      (multiplier != 0 && value > (limit - addition) / multiplier)) {
    return Status(ErrorCode::kOverflow);
  }
  return value * multiplier + addition;
}

inline bool SupportedLimit(extent_t limit) {
  return limit == std::numeric_limits<std::int32_t>::max() ||
         limit == std::numeric_limits<std::int64_t>::max();
}

// Nonempty execution has unit-increment row/column loops and I+1 reflector
// addresses. Blocked routines also increment reflector loops by NB=32.
inline Status CheckIterationBounds(Operation operation, extent_t rows,
                                   extent_t columns, extent_t reflectors,
                                   extent_t limit) {
  if (rows == 0 || columns == 0 ||
      (operation == Operation::kApply && reflectors == 0)) {
    return Status::Ok();
  }
  const extent_t step = operation == Operation::kGeqr2 ? 1 : 32;
  if (rows > limit - 1 || columns > limit - 1 || reflectors > limit - step) {
    return Status(ErrorCode::kOverflow);
  }
  return Status::Ok();
}

// Reviewed ILAENV branches use NB=32 for GEQRF and ORG/UNG/ORM/UNM QR.
// ORM/UNM's fixed T storage is (NBMAX+1)*NBMAX = 65*64 = 4160, even when
// ILAENV selects 32. Preserve source arithmetic before its LWORK query return.
inline Result<Counts> QueryCounts(Operation operation, extent_t rows,
                                  extent_t columns, extent_t reflectors,
                                  bool left, extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rows < 0 || columns < 0 || reflectors < 0 || rows > limit ||
      columns > limit || reflectors > limit) {
    return Status(ErrorCode::kOverflow);
  }
  const Status iterations =
      CheckIterationBounds(operation, rows, columns, reflectors, limit);
  if (!iterations.ok()) {
    return iterations;
  }
  if (operation == Operation::kGeqrf || operation == Operation::kGeqr2) {
    if (reflectors != std::min(rows, columns)) {
      return Status(ErrorCode::kShape);
    }
    if (operation == Operation::kGeqr2) {
      return Counts{columns, columns};
    }
    if (reflectors == 0) {
      return Counts{1, 1};
    }
    const auto preferred = MultiplyAdd(columns, 32, 0, limit);
    if (!preferred.ok()) {
      return preferred.status();
    }
    return Counts{columns, *preferred};
  }
  if (operation == Operation::kGenerate) {
    if (columns > rows || reflectors > columns) {
      return Status(ErrorCode::kShape);
    }
    const auto minimum = std::max<extent_t>(1, columns);
    const auto preferred = MultiplyAdd(minimum, 32, 0, limit);
    if (!preferred.ok()) {
      return preferred.status();
    }
    return Counts{minimum, *preferred};
  }
  if (operation != Operation::kApply) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (reflectors > (left ? rows : columns)) {
    return Status(ErrorCode::kShape);
  }
  const auto minimum = std::max<extent_t>(1, left ? columns : rows);
  const auto preferred = MultiplyAdd(minimum, 32, 4160, limit);
  if (!preferred.ok()) {
    return preferred.status();
  }
  return Counts{minimum, *preferred};
}

// LARF1F/LARFB pass rows of the transformed matrix to AXPY/SCAL/COPY.
// Their final INTEGER cursor is 1 + length*LDC, not the last live address.
// The first unblocked update has the largest tail, N-1; this also dominates
// blocked trailing updates. Original ASC row strides must not be passed here.
inline Status RowCursorBounds(Operation operation, extent_t rows,
                              extent_t columns, extent_t reflectors, bool left,
                              bool complex, extent_t leading, extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (rows < 0 || columns < 0 || reflectors < 0 ||
      leading < std::max<extent_t>(1, rows) || leading > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (rows == 0 || columns == 0 || reflectors == 0) {
    return Status::Ok();
  }
  extent_t tail = 0;
  if (operation == Operation::kApply) {
    tail = left ? columns : 0;
  } else if (operation == Operation::kGenerate) {
    tail = columns - 1;
  } else if (operation == Operation::kGeqrf || operation == Operation::kGeqr2) {
    // Real LARFG of an order-one vector necessarily produces TAU=0.
    // Complex LARFG may instead produce a nontrivial phase reflector.
    tail = complex || rows > 1 ? columns - 1 : 0;
  } else {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto cursor = MultiplyAdd(tail, leading, 1, limit);
  return cursor.ok() ? Status::Ok() : cursor.status();
}

// Validate the pinned query's floating arithmetic before foreign execution.
// S/C calls SROUNDUP_LWORK, which converts REAL(LWORK) back to its INTEGER
// before deciding whether to multiply upward. Both conversions must fit.
// D/Z QR assigns integer LWKOPT directly to double. The returned floating
// value can round down, so the independently checked raw integer remains a
// lower bound on the final preferred capacity, not merely ceil(float(raw)).
template <typename Real>
  requires(std::same_as<Real, float> || std::same_as<Real, double>)
Result<extent_t> GuardQueryCapacity(extent_t raw, extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (raw < 0 || raw > limit) {
    return Status(ErrorCode::kOverflow);
  }
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

// This capacity is ASC-owned, not a provider LWORK count. Byte limits are
// checked before pointer arithmetic; zero products require no allocation.
inline Result<extent_t> AppendPacking(extent_t existing, extent_t rows,
                                      extent_t columns, std::size_t bytes) {
  if (bytes == 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto count = MultiplyAdd(rows, columns, existing,
                                 std::numeric_limits<extent_t>::max());
  if (!count.ok()) {
    return count.status();
  }
  if (static_cast<std::uint64_t>(*count) >
      std::numeric_limits<std::size_t>::max() / bytes) {
    return Status(ErrorCode::kOverflow);
  }
  return *count;
}

}  // namespace asc::internal_lapack_qr

#endif  // ASC_DENSE_LAPACK_INTERNAL_QR_COUNTS_H_
