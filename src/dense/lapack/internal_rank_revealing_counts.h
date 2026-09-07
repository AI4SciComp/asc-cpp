#ifndef ASC_DENSE_LAPACK_INTERNAL_RANK_REVEALING_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_RANK_REVEALING_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

// Private source-pinned count arithmetic. Tests use actual integer values,
// never fabricated backing spans. No allocation or foreign call occurs here.
namespace asc::internal_lapack_rank_revealing {

enum class Operation : std::uint8_t { kGeqp3, kGelsy };

struct Counts {
  extent_t minimum;
  extent_t preferred;
  extent_t raw_preferred;
  extent_t returned_preferred;
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

// SROUNDUP first converts REAL(raw) back to INTEGER, then may multiply by
// 1+epsilon. Both conversions must fit. Plain C/Z outer queries do not use
// SROUNDUP; preserve their returned rounding independently from capacities.
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
Result<Counts> FinalCounts(extent_t minimum, extent_t preferred, bool complex,
                           extent_t limit) {
  const auto returned = ReturnedInteger<Real>(
      preferred, std::same_as<Real, float> && !complex, limit);
  if (!returned.ok()) {
    return returned.status();
  }
  return Counts{minimum, std::max({minimum, preferred, *returned}), preferred,
                *returned};
}

// Fixed-column GEQRF and ORM/UNMQR write WORK(1), then GEQP3 converts it to
// INTEGER even with minimum workspace. Bound every possible fixed-flag choice.
// The final SGEQP3 SROUNDUP can round the already converted IWS a second time.
template <typename Real>
Status FixedColumnQueries(extent_t k, extent_t n, extent_t outer, bool complex,
                          extent_t limit) {
  const auto factor = MultiplyAdd(k, 32, 0, limit);
  if (!factor.ok()) {
    return factor.status();
  }
  const auto converted =
      ReturnedInteger<Real>(*factor, std::same_as<Real, float>, limit);
  if (!converted.ok()) {
    return converted.status();
  }
  extent_t iws = std::max(outer, *converted);
  if (n > 1) {
    const auto apply = MultiplyAdd(n - 1, 32, 4160, limit);
    if (!apply.ok()) {
      return apply.status();
    }
    const auto value =
        ReturnedInteger<Real>(*apply, std::same_as<Real, float>, limit);
    if (!value.ok()) {
      return value.status();
    }
    iws = std::max(iws, *value);
  }
  const auto final =
      ReturnedInteger<Real>(iws, std::same_as<Real, float> && !complex, limit);
  return final.ok() ? Status::Ok() : final.status();
}

template <typename Real>
Result<Counts> ZeroRowGeqp3Counts(extent_t n, extent_t limit) {
  if (n == limit) {
    return Status(ErrorCode::kOverflow);
  }
  // GEQP3's outer query is one, but fixed columns call ORM/UNMQR(0,n,0).
  // That validates LWORK>=n and computes this preferred INTEGER expression
  // before its own quick return overwrites WORK(1) with one. S/C additionally
  // run SROUNDUP before that overwrite; D/Z do not reconvert that intermediate.
  const auto nested = MultiplyAdd(n, 32, 4160, limit);
  if (!nested.ok()) {
    return nested.status();
  }
  if constexpr (std::same_as<Real, float>) {
    const auto rounded = ReturnedInteger<Real>(*nested, true, limit);
    if (!rounded.ok()) {
      return rounded.status();
    }
  }
  return Counts{n, n, 1, 1};
}

template <typename Real>
Result<Counts> Geqp3Counts(extent_t m, extent_t n, extent_t lda, bool complex,
                           extent_t limit) {
  const extent_t k = std::min(m, n);
  if (n == 0) {
    return Counts{1, 1, 1, 1};
  }
  if (m == 0) {
    return ZeroRowGeqp3Counts<Real>(n, limit);
  }
  if (m == limit || n == limit) {
    return Status(ErrorCode::kOverflow);
  }
  const auto minimum = MultiplyAdd(n, complex ? 1 : 3, 1, limit);
  const auto preferred = MultiplyAdd(n, complex ? 32 : 34, 32, limit);
  if (!minimum.ok() || !preferred.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  // Pinned LAQP2/GEQR2 use LARF1F, not the older LARF. The real left
  // application AXPYs the first C row with stride LDA; complex LASTV=1
  // SCALs it. Include the final cursor. A real one-row factor has tau=0
  // for every reflector, so that exact source mode does not consume a row.
  if ((complex || m > 1) && !MultiplyAdd(n - 1, lda, 1, limit).ok()) {
    return Status(ErrorCode::kOverflow);
  }
  // Pinned NB=32/NX=128 permits LAQPS only above this reflector count.
  // LAQPS stores linked-list column indices in VN2's real scalar, then NINTs
  // them. Its real GEMV row cursor is covered by the preceding bound.
  if (k > 128) {
    constexpr extent_t kExactIndexLimit = extent_t{1}
                                          << std::numeric_limits<Real>::digits;
    if (n > kExactIndexLimit) {
      return Status(ErrorCode::kOverflow);
    }
  }
  const Status fixed =
      FixedColumnQueries<Real>(k, n, *preferred, complex, limit);
  if (!fixed.ok()) {
    return fixed;
  }
  return FinalCounts<Real>(*minimum, *preferred, complex, limit);
}

template <typename Real>
Result<Counts> GelsyCounts(extent_t m, extent_t n, extent_t nrhs, extent_t lda,
                           bool complex, extent_t limit) {
  const extent_t k = std::min(m, n);
  // All four calculate these indices before query and quick return.
  const auto ismax = MultiplyAdd(k, 2, 1, limit);
  if (!ismax.ok()) {
    return ismax.status();
  }
  const bool empty = k == 0 || nrhs == 0;
  if (empty && !complex) {
    return Counts{1, 1, 1, 1};
  }
  const auto n_plus_one = MultiplyAdd(n, 1, 1, limit);
  const auto k_plus_rhs = MultiplyAdd(k, 1, nrhs, limit);
  if (!n_plus_one.ok() || !k_plus_rhs.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto outer_minimum =
      MultiplyAdd(k, 1, std::max({2 * k, *n_plus_one, *k_plus_rhs}), limit);
  const auto first = MultiplyAdd(n, 34, 32, limit);
  const auto second = MultiplyAdd(nrhs, 32, 2 * k, limit);
  if (!outer_minimum.ok() || !first.ok() || !second.ok()) {
    return Status(ErrorCode::kOverflow);
  }
  const auto full_first = MultiplyAdd(k, 1, *first, limit);
  if (!full_first.ok()) {
    return full_first.status();
  }
  const extent_t preferred = std::max({extent_t{1}, *full_first, *second,
                                       complex ? extent_t{1} : *outer_minimum});
  extent_t minimum = *outer_minimum;
  if (!empty) {
    if (m == limit || n == limit || nrhs == limit) {
      return Status(ErrorCode::kOverflow);
    }
    const auto qr = Geqp3Counts<Real>(m, n, lda, complex, limit);
    const auto cursor = MultiplyAdd(n - 1, lda, 1, limit);
    const auto apply = MultiplyAdd(nrhs, 32, 4160, limit);
    if (!qr.ok() || !cursor.ok() || !apply.ok()) {
      return Status(ErrorCode::kOverflow);
    }
    const auto converted =
        ReturnedInteger<Real>(*apply, std::same_as<Real, float>, limit);
    if (!converted.ok()) {
      return converted.status();
    }
    // The real executable outer minimum is insufficient for its nested
    // GEQP3. This independently derived full-call minimum prevents XERBLA.
    const auto nested_minimum = MultiplyAdd(k, 1, qr->minimum, limit);
    if (!nested_minimum.ok()) {
      return nested_minimum.status();
    }
    minimum = std::max(minimum, *nested_minimum);
  }
  const auto result = FinalCounts<Real>(minimum, preferred, complex, limit);
  if (!result.ok()) {
    return result.status();
  }
  // GELSY packs B with LDB=max(m,n). Even unblocked ORM2R uses LARF1F's
  // first-row AXPY/SCAL; ORMR3 can COPY it through LARZ after a wide/rank-
  // deficient RZ factorization. Only the real scalar A has neither route.
  if (!empty && (complex || std::max(m, n) > 1) &&
      !MultiplyAdd(nrhs, std::max(m, n), 1, limit).ok()) {
    return Status(ErrorCode::kOverflow);
  }
  return *result;
}

template <typename Real>
Result<Counts> QueryCounts(Operation operation, extent_t m, extent_t n,
                           extent_t nrhs, extent_t lda, bool complex,
                           extent_t limit) {
  if (!SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m < 0 || n < 0 || nrhs < 0 || m > limit || n > limit || nrhs > limit ||
      lda < std::max<extent_t>(1, m) || lda > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (operation == Operation::kGeqp3) {
    return Geqp3Counts<Real>(m, n, lda, complex, limit);
  }
  if (operation == Operation::kGelsy) {
    return GelsyCounts<Real>(m, n, nrhs, lda, complex, limit);
  }
  return Status(ErrorCode::kInvalidArgument);
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

}  // namespace asc::internal_lapack_rank_revealing

#endif  // ASC_DENSE_LAPACK_INTERNAL_RANK_REVEALING_COUNTS_H_
