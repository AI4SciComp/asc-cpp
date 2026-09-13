#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_COUNTS_H_

#include <algorithm>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_rank_revealing_counts.h"
#include "internal_svd_least_squares_counts.h"

namespace asc::internal_dmd_counts {

// Minimum resource predecessor only. This is NOT a native-entry guard: nested
// preferred-query arithmetic and downstream SVD/eigensolver bounds still need
// validation before any future public GEDMD call can be admitted.
struct MinimumCounts {
  extent_t scalar = 0;  // Independently sufficient capacity for these formulas.
  extent_t real = 0;
  extent_t integer = 0;
  extent_t source_scalar = 0;  // After the native nested INT conversions.
  extent_t source_real = 0;
  extent_t query_scalar = 0;  // Encoded outer minimum, not preferred workspace.
  extent_t query_real = 0;
};

using Arithmetic = internal_lapack_svd_least_squares::Arithmetic;

template <typename Real>
MinimumCounts RealMinimum(extent_t m, extent_t n, int svd, bool eigenvectors,
                          Arithmetic& a) {
  MinimumCounts counts;
  counts.integer = 1;
  const auto two_n = a.Product(2, n);
  const auto three_n = a.Product(3, n);
  const auto four_n = a.Product(4, n);
  const auto five_n = a.Product(5, n);
  extent_t scalar = 2;
  extent_t source_scalar = 2;
  if (svd == 1) {
    scalar = a.Sum({n, std::max(a.Sum({m, three_n}), five_n)});
    source_scalar = scalar;
  } else if (svd == 2) {
    const auto square = a.Product(n, n);
    scalar = a.Sum({n, a.Product(3, square),
                    std::max(m, a.Sum({a.Product(5, square), four_n}))});
    source_scalar = scalar;
    counts.integer = a.Product(8, n);
  } else if (svd == 3) {
    const auto inner = a.Product(6, n);
    const auto inner_real = std::max<extent_t>(m, 2);
    scalar = a.Sum({n, inner, inner_real});
    source_scalar = a.Sum(
        {n, a.Round<Real>(inner, false), a.Round<Real>(inner_real, false)});
    counts.integer = a.Sum({m, n - 1});
  } else {
    const auto square = a.Product(n, n);
    scalar = a.Sum(
        {n, std::max({extent_t{7}, a.Sum({a.Product(2, m), n}),
                      a.Sum({square, four_n}), a.Sum({square, two_n, 6})})});
    source_scalar = scalar;
    counts.integer = std::max<extent_t>(3, a.Sum({m, three_n}));
  }
  const auto eigen = a.Sum({n, eigenvectors ? four_n : three_n});
  scalar = std::max({extent_t{2}, scalar, eigen});
  source_scalar = std::max({extent_t{2}, source_scalar, eigen});
  counts.scalar = std::max(scalar, source_scalar);
  counts.source_scalar = source_scalar;
  return counts;
}

template <typename Real>
MinimumCounts ComplexMinimum(extent_t m, extent_t n, int svd, Arithmetic& a) {
  MinimumCounts counts;
  counts.integer = 1;
  const auto two_n = a.Product(2, n);
  const auto three_n = a.Product(3, n);
  const auto four_n = a.Product(4, n);
  const auto five_n = a.Product(5, n);
  extent_t scalar = 2;
  extent_t real = 0;
  extent_t source_scalar = 2;
  extent_t source_real = 0;
  if (svd == 1) {
    scalar = a.Sum({m, two_n});
    real = a.Product(6, n);
  } else if (svd == 2) {
    const auto square = a.Product(n, n);
    const auto two_square = a.Product(2, square);
    scalar = a.Sum({two_square, two_n, m});
    real = a.Sum(
        {n, std::max(a.Sum({a.Product(5, square), a.Product(7, n)}),
                     a.Sum({a.Product(2, a.Product(m, n)), two_square, n}))});
    counts.integer = a.Product(8, n);
  } else if (svd == 3) {
    scalar = four_n;
    real = a.Sum({n, std::max(m, five_n)});
    counts.integer = a.Sum({m, n - 1});
  } else {
    scalar = std::max(a.Sum({a.Product(n, n), four_n}), a.Sum({n, m}));
    real = a.Sum({n, std::max<extent_t>(7, a.Product(2, m))});
    counts.integer = std::max<extent_t>(4, a.Sum({m, n}));
  }
  source_scalar = scalar;
  source_real = real;
  if (svd == 3 || svd == 4) {
    source_scalar = a.Round<Real>(scalar, false);
    source_real = a.Sum({n, a.Round<Real>(real - n, false)});
  }
  scalar = std::max({extent_t{2}, scalar, two_n});
  real = std::max(real, three_n);
  source_scalar = std::max({extent_t{2}, source_scalar, two_n});
  source_real = std::max(source_real, three_n);
  counts.scalar = std::max(scalar, source_scalar);
  counts.real = std::max(real, source_real);
  counts.source_scalar = source_scalar;
  counts.source_real = source_real;
  return counts;
}

// Derive the selected pinned minimum resource branches without array storage
// or a foreign call. Source-rounded minima and sufficient capacities remain
// distinct. No overflow or lost count is hidden by a diagnostic clamp.
template <typename Real>
Result<MinimumCounts> Minimum(extent_t m, extent_t n, int svd,
                              bool eigenvectors, bool complex, extent_t limit) {
  if (m < 0 || n < 0 || n > m || svd < 1 || svd > 4 ||
      !internal_lapack_rank_revealing::SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n == 0) {
    MinimumCounts counts;
    // Native query defaults with INFO=1; local execution is a separate
    // contract.
    counts.scalar = counts.source_scalar = counts.query_scalar = 2;
    counts.real = counts.source_real = counts.query_real = complex ? 1 : 0;
    counts.integer = 1;
    return counts;
  }
  Arithmetic a(limit);
  MinimumCounts counts = complex
                             ? ComplexMinimum<Real>(m, n, svd, a)
                             : RealMinimum<Real>(m, n, svd, eigenvectors, a);
  counts.query_scalar = a.Round<Real>(counts.source_scalar, false);
  counts.query_real = a.Round<Real>(counts.source_real, false);
  return a.valid() ? Result<MinimumCounts>(counts)
                   : Result<MinimumCounts>(Status(ErrorCode::kOverflow));
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_COUNTS_H_
