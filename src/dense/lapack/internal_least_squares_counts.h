#ifndef ASC_DENSE_LAPACK_INTERNAL_LEAST_SQUARES_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_LEAST_SQUARES_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

// Private, source-pinned arithmetic. Pure tests use real integer values,
// never fabricated backing spans. No allocation or foreign invocation.
namespace asc::internal_lapack_least_squares {

enum class Operation : std::uint8_t { kGels, kGelst, kGetsls };

struct Counts {
  extent_t minimum;
  extent_t preferred;
  extent_t raw_minimum;
  extent_t raw_preferred;
};

inline bool SupportedLimit(extent_t limit) {
  return limit == std::numeric_limits<std::int32_t>::max() ||
         limit == std::numeric_limits<std::int64_t>::max();
}

inline Result<extent_t> MultiplyAdd(extent_t a, extent_t b, extent_t c,
                                    extent_t limit) {
  if (a < 0 || b < 0 || c < 0 || c > limit || (b != 0 && a > (limit - c) / b)) {
    return Status(ErrorCode::kOverflow);
  }
  return a * b + c;
}

// Models the actual returned scalar, including SROUNDUP's initial INTEGER
// reconversion and optional epsilon multiplication. Callers distinguish the
// actual rounded integer from max(raw,returned) storage requirements.
template <typename Real>
  requires(std::same_as<Real, float> || std::same_as<Real, double>)
Result<extent_t> ReturnedInteger(extent_t raw, bool upward, extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (raw < 0 || raw > limit) {
    return Status(ErrorCode::kOverflow);
  }
  const Real bound = std::ldexp(
      Real{1}, limit == std::numeric_limits<std::int32_t>::max() ? 31 : 63);
  Real value = static_cast<Real>(raw);
  if (value >= bound) {
    return Status(ErrorCode::kOverflow);
  }
  if (upward && static_cast<extent_t>(value) < raw) {
    value *= Real{1} + std::numeric_limits<Real>::epsilon();
    if (value >= bound) {
      return Status(ErrorCode::kOverflow);
    }
  }
  return static_cast<extent_t>(value);
}

template <typename Real>
Result<extent_t> NestedCapacity(extent_t raw, bool upward, extent_t limit) {
  const auto value = ReturnedInteger<Real>(raw, upward, limit);
  if (!value.ok()) {
    return value.status();
  }
  // GETSLS converts this value internally before the checked ASC boundary
  // regains control. An underreported T or WORK size is unsafe, not repaired
  // by rounding only the outer query result.
  if (*value < raw) {
    return Status(ErrorCode::kOverflow);
  }
  return *value;
}

template <typename Real>
Status ExactMetadata(extent_t raw, extent_t limit) {
  const auto value = ReturnedInteger<Real>(raw, false, limit);
  if (!value.ok()) {
    return value.status();
  }
  return *value == raw ? Status::Ok() : Status(ErrorCode::kOverflow);
}

template <typename Real>
Result<Counts> FinalCounts(extent_t minimum, extent_t preferred, bool upward,
                           extent_t limit, bool minimum_query) {
  const auto high = ReturnedInteger<Real>(preferred, upward, limit);
  if (!high.ok()) {
    return high.status();
  }
  const auto low = minimum_query ? ReturnedInteger<Real>(minimum, upward, limit)
                                 : Result<extent_t>(minimum);
  if (!low.ok()) {
    return low.status();
  }
  return Counts{std::max(minimum, *low), std::max(preferred, *high), minimum,
                preferred};
}

// All pinned precisions use inner block NB=1 for GEQR/GELQ. ILAENV computes
// M*N before selecting a tall-skinny outer MB, even if M<=8192 also holds.
// The wide branch always selects outer NB=N after its source normalization:
// M<=8192 returns M; otherwise 32768/N<=M. No algorithm is substituted here.
template <typename Real>
Result<Counts> GetslsCounts(extent_t m, extent_t n, extent_t nrhs,
                            extent_t limit) {
  constexpr bool kSingle = std::same_as<Real, float>;
  if (std::min({m, n, nrhs}) == 0) {
    return Counts{1, 1, 1, 1};
  }
  const auto product = MultiplyAdd(m, n, 0, limit);
  if (!product.ok()) {
    return product.status();
  }
  extent_t outer = m;
  extent_t blocks = 1;
  if (m >= n) {
    if (*product > 131072 && m > 8192) {
      outer = 32768 / n;
    }
    if (outer > m || outer <= n) {
      outer = m;
    }
    if (outer > n && m > n) {
      blocks = (m - n) / (outer - n);
      if ((m - n) % (outer - n) != 0) {
        ++blocks;
      }
    }
  } else {
    outer = n;
  }
  // Preferred and reduced-T execution can store different outer metadata.
  // Loss would change GEMQR/GEMLQ's algorithm/offset selection.
  for (const extent_t metadata : {outer, std::max(m, n)}) {
    Status exact = ExactMetadata<Real>(metadata, limit);
    if (!exact.ok()) {
      return exact;
    }
  }
  const extent_t k = std::min(m, n);
  const auto t_opt_raw = MultiplyAdd(k, blocks, 5, limit);
  const auto t_min_raw = MultiplyAdd(k, 1, 5, limit);
  if (!t_opt_raw.ok() || !t_min_raw.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto t_opt = NestedCapacity<Real>(*t_opt_raw, false, limit);
  const auto t_min = NestedCapacity<Real>(*t_min_raw, false, limit);
  // GEQR needs N; pinned GELQ's normalized full-width route also needs N.
  const auto factor = NestedCapacity<Real>(n, kSingle, limit);
  const auto apply = NestedCapacity<Real>(nrhs, kSingle, limit);
  if (!t_opt.ok() || !t_min.ok() || !factor.ok() || !apply.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto work = std::max(*factor, *apply);
  const auto minimum = MultiplyAdd(*t_min, 1, work, limit);
  const auto preferred = MultiplyAdd(*t_opt, 1, work, limit);
  if (!minimum.ok() || !preferred.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  return FinalCounts<Real>(*minimum, *preferred, kSingle, limit, true);
}

// The source's left reflector application passes a row to strided BLAS;
// the final cursor advance is required even after the last accessed value.
// A uses its effective foreign LDA; B is always packed with LD=max(M,N).
// Dimensions and the existing wide-row reflector bound are validated first.
inline Status ReflectorCursorBounds(Operation operation, extent_t m, extent_t n,
                                    extent_t nrhs, extent_t lda, bool complex,
                                    extent_t limit) {
  if (std::min(m, n) == 0 || nrhs == 0) {
    return Status::Ok();
  }
  if (m >= n) {
    const auto factor = MultiplyAdd(n - 1, lda, 1, limit);
    if (!factor.ok()) {
      return factor.status();
    }
  }
  // Real scalar GELS has TAU=0. Compact-WY uses LARFB's copy regardless;
  // complex scalar GELS can have a nontrivial phase reflector.
  if (operation != Operation::kGels || complex || std::max(m, n) > 1) {
    const auto rhs = MultiplyAdd(nrhs, std::max(m, n), 1, limit);
    if (!rhs.ok()) {
      return rhs.status();
    }
  }
  return Status::Ok();
}

template <typename Real>
Result<Counts> QueryCounts(Operation operation, extent_t m, extent_t n,
                           extent_t nrhs, extent_t lda, bool complex,
                           extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m < 0 || n < 0 || nrhs < 0 || lda < std::max<extent_t>(1, m) ||
      m > limit || n > limit || nrhs > limit || lda > limit) {
    return Status(ErrorCode::kOverflow);
  }
  const extent_t k = std::min(m, n);
  if (nrhs != 0 && std::max(m, n) != 0 &&
      (m == limit || n == limit || nrhs == limit)) {
    // LASET/norm/scaling and reflector loops require their terminal +1.
    return Status(ErrorCode::kOverflow);
  }
  if (k != 0 && nrhs != 0 && m < n) {
    // Row reflectors have foreign vector increment LDA. Complex GELS also
    // calls LACGV over the full row and advances its cursor after the last
    // value. Other row-reflector norm/scaling paths consume N-1 tail values.
    const auto cursor =
        MultiplyAdd(n - ((operation == Operation::kGels && complex) ? 0 : 1),
                    lda, 1, limit);
    if (!cursor.ok()) {
      return cursor.status();
    }
  }
  const Status cursors =
      ReflectorCursorBounds(operation, m, n, nrhs, lda, complex, limit);
  if (!cursors.ok()) {
    return cursors;
  }
  if (operation == Operation::kGetsls) {
    return GetslsCounts<Real>(m, n, nrhs, limit);
  }
  if (operation != Operation::kGels && operation != Operation::kGelst) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto minimum = MultiplyAdd(k, 1, std::max(k, nrhs), limit);
  if (!minimum.ok()) {
    return minimum.status();
  }
  extent_t preferred = std::max<extent_t>(1, *minimum);
  if (operation == Operation::kGels) {
    const auto raw = MultiplyAdd(std::max(k, nrhs), 32, k, limit);
    if (!raw.ok()) {
      return raw.status();
    }
    preferred = std::max<extent_t>(1, *raw);
    if (k != 0 && nrhs != 0) {
      // Nested ORM/UNM QR/LQ calculates NW*32+65*64 even with minimum WORK.
      const auto apply = MultiplyAdd(nrhs, 32, 4160, limit);
      if (!apply.ok()) {
        return apply.status();
      }
      const auto converted =
          ReturnedInteger<Real>(*apply, std::same_as<Real, float>, limit);
      if (!converted.ok()) {
        return converted.status();
      }
    }
  }
  const bool upward =
      std::same_as<Real, float> && (operation != Operation::kGels || !complex);
  return FinalCounts<Real>(std::max<extent_t>(1, *minimum), preferred, upward,
                           limit, false);
}

inline Result<extent_t> AppendPacking(extent_t existing, extent_t m, extent_t n,
                                      std::size_t bytes) {
  if (bytes == 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  const auto count =
      MultiplyAdd(m, n, existing, std::numeric_limits<extent_t>::max());
  if (!count.ok()) {
    return count.status();
  }
  if (static_cast<std::uint64_t>(*count) >
      std::numeric_limits<std::size_t>::max() / bytes) {
    return Status(ErrorCode::kOverflow);
  }
  return *count;
}

}  // namespace asc::internal_lapack_least_squares

#endif  // ASC_DENSE_LAPACK_INTERNAL_LEAST_SQUARES_COUNTS_H_
