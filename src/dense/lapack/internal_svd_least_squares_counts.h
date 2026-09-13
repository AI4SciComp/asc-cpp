#ifndef ASC_DENSE_LAPACK_INTERNAL_SVD_LEAST_SQUARES_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_SVD_LEAST_SQUARES_COUNTS_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_least_squares_counts.h"

// Private arithmetic for the exact pinned eight source routines. This is not
// a public provider query, and synthetic tests never forge matrix backing.
namespace asc::internal_lapack_svd_least_squares {

enum class Operation : std::uint8_t { kGelss, kGelsd };

struct Counts {
  extent_t minimum = 1;
  extent_t source_minimum = 1;
  extent_t preferred = 1;
  extent_t scalar_query = 1;
  extent_t real = 0;
  extent_t real_query = 0;
  extent_t integer = 0;
};

// Sticky bounded arithmetic keeps formulas legible without allocating error
// messages. After failure, zero is only a placeholder; Finish rejects it.
class Arithmetic {
 public:
  explicit Arithmetic(extent_t limit) : limit_(limit) {}

  extent_t Sum(std::initializer_list<extent_t> values) {
    extent_t result = 0;
    for (const extent_t value : values) {
      if (value < 0 || value > limit_ - result) {
        valid_ = false;
        return 0;
      }
      result += value;
    }
    return result;
  }

  extent_t Product(extent_t a, extent_t b) {
    const auto value =
        internal_lapack_least_squares::MultiplyAdd(a, b, 0, limit_);
    if (!value.ok()) {
      valid_ = false;
      return 0;
    }
    return *value;
  }

  template <typename Real>
  extent_t Round(extent_t raw, bool upward = std::is_same_v<Real, float>) {
    const auto value = internal_lapack_least_squares::ReturnedInteger<Real>(
        raw, upward, limit_);
    if (!value.ok()) {
      valid_ = false;
      return 0;
    }
    return *value;
  }

  // ILAENV(6) uses default REAL, even for D/Z and true INTEGER64 builds.
  extent_t Crossover(extent_t k) {
    const float value = static_cast<float>(k) * 1.6F;
    const float bound = std::ldexp(
        1.F, limit_ == std::numeric_limits<std::int32_t>::max() ? 31 : 63);
    if (!std::isfinite(value) || value < 0 || value >= bound) {
      valid_ = false;
      return 0;
    }
    return static_cast<extent_t>(value);
  }

  [[nodiscard]] bool valid() const { return valid_; }

 private:
  extent_t limit_;
  bool valid_ = true;
};

template <typename Real>
extent_t Levels(extent_t k) {
  // Source INT truncates toward zero, not floor. In particular k=14..25
  // gives one, despite being below the 26-entry divide threshold.
  const Real quotient = static_cast<Real>(std::max<extent_t>(1, k)) / Real{26};
  const Real logarithm = std::log(quotient) / std::log(Real{2});
  return std::max<extent_t>(0, static_cast<extent_t>(logarithm) + 1);
}

template <typename Real>
extent_t BidiagonalQQuery(extent_t nrhs, bool complex, Arithmetic& arithmetic) {
  if (complex && nrhs == 0) {
    return 1;
  }
  return arithmetic.Round<Real>(
      arithmetic.Product(std::max<extent_t>(1, nrhs), 32));
}

template <typename Real>
extent_t ApplyQuery(extent_t nrhs, bool complex, Arithmetic& arithmetic) {
  if (complex && std::is_same_v<Real, float> && nrhs == 0) {
    return 1;
  }
  return arithmetic.Round<Real>(arithmetic.Sum(
      {arithmetic.Product(std::max<extent_t>(1, nrhs), 32), 4160}));
}

template <typename Real>
extent_t FormPQuery(extent_t k, bool rectangular, Arithmetic& arithmetic) {
  extent_t query = 1;
  if (rectangular || k > 1) {
    query =
        arithmetic.Round<Real>(arithmetic.Product(rectangular ? k : k - 1, 32));
  }
  // ORG/UNGBR itself INTs its nested result, applies MAX(MN), then rounds
  // again. GELSS performs another INT after this second result.
  return arithmetic.Round<Real>(std::max(k, query));
}

template <typename Real>
Counts GelssCounts(extent_t m, extent_t n, extent_t nrhs, bool complex,
                   extent_t crossover, Arithmetic& arithmetic) {
  Counts counts;
  const extent_t k = std::min(m, n);
  if (k == 0) {
    return counts;
  }
  const extent_t block = complex ? 2 : 3;
  const extent_t offset = arithmetic.Product(block, k);
  const extent_t qr = arithmetic.Round<Real>(arithmetic.Product(k, 32));
  const extent_t apply = ApplyQuery<Real>(nrhs, complex, arithmetic);
  const extent_t bidiag_q = BidiagonalQQuery<Real>(nrhs, complex, arithmetic);
  extent_t maximum = 1;
  if (m >= n) {
    const extent_t mm = m >= crossover ? n : m;
    if (m >= crossover) {
      // C/Z calculate but do not use the nested GEQRF/UNMQR query results.
      maximum =
          complex
              ? std::max(arithmetic.Product(n, 33),
                         arithmetic.Sum({n, arithmetic.Product(nrhs, 32)}))
              : std::max(arithmetic.Sum({n, qr}), arithmetic.Sum({n, apply}));
    }
    const extent_t bidiag =
        arithmetic.Round<Real>(arithmetic.Product(arithmetic.Sum({mm, n}), 32));
    maximum = std::max(
        {maximum, arithmetic.Sum({offset, bidiag}),
         arithmetic.Sum({offset, bidiag_q}),
         arithmetic.Sum({offset, FormPQuery<Real>(n, false, arithmetic)}),
         arithmetic.Product(n, nrhs), complex ? 1 : arithmetic.Product(n, 5)});
    counts.minimum = complex ? arithmetic.Sum({offset, std::max(m, nrhs)})
                             : std::max({arithmetic.Sum({offset, mm}),
                                         arithmetic.Sum({offset, nrhs}),
                                         arithmetic.Product(n, 5)});
  } else {
    counts.minimum = arithmetic.Sum({offset, std::max(n, nrhs)});
    if (!complex) {
      counts.minimum = std::max(counts.minimum, arithmetic.Product(m, 5));
    }
    if (n >= crossover) {
      const extent_t square = arithmetic.Product(m, m);
      const extent_t base =
          arithmetic.Sum({square, arithmetic.Product(m, complex ? 3 : 4)});
      const extent_t bidiag = arithmetic.Round<Real>(arithmetic.Product(m, 64));
      const extent_t lq = !complex && std::is_same_v<Real, float>
                              ? arithmetic.Product(m, 32)
                              : qr;
      maximum = std::max(
          {arithmetic.Sum({m, lq}), arithmetic.Sum({base, bidiag}),
           arithmetic.Sum({base, bidiag_q}),
           arithmetic.Sum({base, FormPQuery<Real>(m, false, arithmetic)}),
           arithmetic.Sum(
               {square, m, arithmetic.Product(m, nrhs > 1 ? nrhs : 1)}),
           arithmetic.Sum({m, apply}),
           complex ? 1 : arithmetic.Sum({square, arithmetic.Product(m, 6)})});
    } else {
      const extent_t bidiag = arithmetic.Round<Real>(
          arithmetic.Product(arithmetic.Sum({m, n}), 32));
      maximum = std::max(
          {arithmetic.Sum({offset, bidiag}), arithmetic.Sum({offset, bidiag_q}),
           arithmetic.Sum({offset, FormPQuery<Real>(m, true, arithmetic)}),
           arithmetic.Product(n, nrhs),
           complex ? 1 : arithmetic.Product(m, 5)});
    }
  }
  maximum = std::max(counts.minimum, maximum);
  counts.scalar_query = arithmetic.Round<Real>(maximum);
  counts.preferred = std::max(maximum, counts.scalar_query);
  counts.real = complex ? arithmetic.Product(k, 5) : 0;
  counts.source_minimum = counts.minimum;
  return counts;
}

template <typename Real>
extent_t GelsdWideMaximum(extent_t m, extent_t n, extent_t nrhs, bool complex,
                          extent_t crossover, extent_t real_subproblem,
                          Arithmetic& arithmetic) {
  const extent_t offset = arithmetic.Product(m, complex ? 2 : 3);
  if (n < crossover) {
    return std::max(
        {arithmetic.Sum(
             {offset, arithmetic.Product(arithmetic.Sum({m, n}), 32)}),
         arithmetic.Sum({offset, arithmetic.Product(nrhs, 32)}),
         arithmetic.Sum({offset, arithmetic.Product(m, 32)}),
         arithmetic.Sum({offset, complex ? arithmetic.Product(m, nrhs)
                                         : real_subproblem})});
  }
  const extent_t square = arithmetic.Product(m, m);
  const extent_t base = arithmetic.Sum({square, arithmetic.Product(m, 4)});
  extent_t maximum = std::max(
      {arithmetic.Product(m, 33),
       arithmetic.Sum({base, arithmetic.Product(m, 64)}),
       arithmetic.Sum({base, arithmetic.Product(nrhs, 32)}),
       m > 0 ? arithmetic.Sum({base, arithmetic.Product(m - 1, 32)}) : 0,
       arithmetic.Sum({square, m, arithmetic.Product(m, nrhs > 1 ? nrhs : 1)}),
       arithmetic.Sum(
           {base, complex ? arithmetic.Product(m, nrhs) : real_subproblem}),
       arithmetic.Sum({base, std::max({m, arithmetic.Product(2, m) - 4, nrhs,
                                       n - arithmetic.Product(3, m)})})});
  if (!complex) {
    maximum =
        std::max(maximum, arithmetic.Sum({m, arithmetic.Product(nrhs, 32)}));
  }
  return maximum;
}

template <typename Real>
Counts GelsdCounts(extent_t m, extent_t n, extent_t nrhs, bool complex,
                   extent_t crossover, Arithmetic& arithmetic) {
  Counts counts;
  const extent_t k = std::min(m, n);
  counts.integer = 1;
  counts.real = complex ? 1 : 0;
  counts.real_query = counts.real;
  if (k == 0 && (complex || std::is_same_v<Real, float>)) {
    return counts;
  }
  const extent_t levels = Levels<Real>(k);
  const extent_t integer_k = std::max<extent_t>(1, k);
  counts.integer = arithmetic.Sum(
      {arithmetic.Product(arithmetic.Product(3, integer_k), levels),
       arithmetic.Product(11, integer_k)});
  const extent_t tree = arithmetic.Product(arithmetic.Product(8, k), levels);
  const extent_t real_subproblem = arithmetic.Sum(
      {arithmetic.Product(k, 59), tree, arithmetic.Product(k, nrhs), 676});
  if (complex) {
    counts.real = arithmetic.Sum(
        {arithmetic.Product(k, 60), tree, arithmetic.Product(nrhs, 75),
         std::max<extent_t>(
             676,
             arithmetic.Sum({arithmetic.Product(n, arithmetic.Sum({1, nrhs})),
                             arithmetic.Product(2, nrhs)}))});
    // RWORK query is plain REAL/DBLE, unlike scalar S/C SROUNDUP.
    counts.real_query = arithmetic.Round<Real>(counts.real, false);
    counts.real = std::max(counts.real, counts.real_query);
  }
  const extent_t offset = arithmetic.Product(k, complex ? 2 : 3);
  extent_t maximum = 1;
  if (m >= n) {
    const extent_t mm = m >= crossover ? n : m;
    if (m >= crossover) {
      maximum = std::max(
          arithmetic.Product(n, complex ? 32 : 33),
          arithmetic.Sum({complex ? 0 : n, arithmetic.Product(nrhs, 32)}));
    }
    maximum = std::max(
        {maximum,
         arithmetic.Sum(
             {offset, arithmetic.Product(arithmetic.Sum({mm, n}), 32)}),
         arithmetic.Sum({offset, arithmetic.Product(nrhs, 32)}),
         n > 0 ? arithmetic.Sum({offset, arithmetic.Product(n - 1, 32)}) : 0,
         arithmetic.Sum({offset, complex ? arithmetic.Product(n, nrhs)
                                         : real_subproblem})});
    counts.minimum = std::max(
        {arithmetic.Sum({offset, mm}),
         arithmetic.Sum({offset, complex ? arithmetic.Product(n, nrhs) : nrhs}),
         complex ? 1 : arithmetic.Sum({offset, real_subproblem})});
  } else {
    maximum = GelsdWideMaximum<Real>(m, n, nrhs, complex, crossover,
                                     real_subproblem, arithmetic);
    counts.minimum = std::max(
        {arithmetic.Sum({offset, complex ? n : m}),
         arithmetic.Sum({offset, complex ? arithmetic.Product(m, nrhs) : nrhs}),
         complex ? 1 : arithmetic.Sum({offset, real_subproblem})});
  }
  counts.minimum = std::min(counts.minimum, maximum);
  counts.source_minimum = counts.minimum;
  if (!complex && m > 0 && m < n) {
    // S/DGELSD's advertised MINWRK omits the N-entry workspace required by
    // the fallback GEBRD. Every admitted LWORK must either support that
    // fallback or enter Path 2a with its complete WLALSD requirement.
    const extent_t fallback = arithmetic.Sum({offset, n});
    extent_t safe = fallback;
    if (n >= crossover) {
      const extent_t path_2a = arithmetic.Sum(
          {arithmetic.Product(m, m), arithmetic.Product(m, 4),
           std::max({m, arithmetic.Product(2, m) - 4, nrhs,
                     n - arithmetic.Product(3, m), real_subproblem})});
      safe = std::min(fallback, path_2a);
    }
    counts.minimum = std::max(counts.minimum, safe);
  }
  counts.scalar_query = arithmetic.Round<Real>(maximum);
  counts.preferred = std::max(maximum, counts.scalar_query);
  return counts;
}

// The source evaluates these workspace tests in INTEGER before it selects
// either wide algorithm. `leading` is M for the route test, and LDA for
// the later optional saved-L leading-dimension test.
template <typename Real>
extent_t WideThreshold(Operation operation, extent_t m, extent_t n,
                       extent_t nrhs, extent_t leading, bool complex,
                       Arithmetic& arithmetic) {
  const extent_t square = arithmetic.Product(m, leading);
  if (operation == Operation::kGelss && complex) {
    return arithmetic.Sum({square, arithmetic.Product(m, 3),
                           std::max({m, nrhs, n - arithmetic.Product(m, 2)})});
  }
  const extent_t base = arithmetic.Sum({square, arithmetic.Product(m, 4)});
  extent_t threshold =
      arithmetic.Sum({base, std::max({m, arithmetic.Product(m, 2) - 4, nrhs,
                                      n - arithmetic.Product(m, 3)})});
  if (operation == Operation::kGelsd && !complex) {
    const extent_t subproblem = arithmetic.Sum(
        {arithmetic.Product(m, 59),
         arithmetic.Product(arithmetic.Product(m, 8), Levels<Real>(m)),
         arithmetic.Product(m, nrhs), 676});
    threshold = std::max(threshold, arithmetic.Sum({base, subproblem}));
  }
  return threshold;
}

template <typename Real>
void ExecutionCounts(Operation operation, extent_t m, extent_t n, extent_t nrhs,
                     extent_t lda, bool complex, extent_t crossover,
                     const Counts& counts, Arithmetic& arithmetic) {
  const extent_t k = std::min(m, n);
  if (k == 0) {
    return;
  }
  // Norm/scaling/unit loops, reflector end addresses, and live workspace
  // offsets include the terminal increment, not just the last access.
  arithmetic.Sum({m, 1});
  arithmetic.Sum({n, 1});
  arithmetic.Sum({nrhs, 1});
  arithmetic.Sum({k, 32});
  arithmetic.Sum({counts.preferred, 1});
  arithmetic.Sum({counts.real, 1});
  arithmetic.Sum({counts.integer, 1});
  const extent_t rhs_product = arithmetic.Product(std::max(m, n), nrhs);
  arithmetic.Sum({rhs_product, 1});

  // ORM/UNM BR's outer NW*NB query hides the nested QR/LQ fixed T block.
  // This arithmetic executes even when the supplied WORK uses an unblocked
  // route. GELSS also evaluates some nested queries but ignores their values.
  arithmetic.Round<Real>(arithmetic.Sum(
      {arithmetic.Product(std::max<extent_t>(1, nrhs), 32), 4160}));

  bool fallback = true;
  bool saved_l = false;
  extent_t saved_leading = m;
  if (m < n) {
    const extent_t threshold =
        WideThreshold<Real>(operation, m, n, nrhs, m, complex, arithmetic);
    saved_l = n >= crossover && counts.preferred >= threshold;
    fallback = n < crossover || counts.minimum < threshold;
    if (saved_l) {
      extent_t padded =
          WideThreshold<Real>(operation, m, n, nrhs, lda, complex, arithmetic);
      if (operation == Operation::kGelsd || !complex) {
        padded =
            std::max(padded, arithmetic.Sum({arithmetic.Product(m, lda), m,
                                             arithmetic.Product(m, nrhs)}));
      }
      if (counts.preferred >= padded) {
        saved_leading = lda;
      }
      if (operation == Operation::kGelss) {
        // LDB*NRHS + IWORK - 1 adds before subtracting. IWORK is
        // M+1+M*LDWORK at this right-singular-vector multiplication.
        arithmetic.Sum(
            {rhs_product, m, arithmetic.Product(m, saved_leading), 1});
      }
    }
    if (fallback) {
      // Actual GEBRD still computes its preferred query with minimum WORK;
      // the outer driver's wide-preferred formula may omit this rectangle.
      arithmetic.Round<Real>(arithmetic.Product(arithmetic.Sum({m, n}), 32));
    }
  }

  extent_t row_length = n - 1;
  if ((operation == Operation::kGelss && (m >= n || fallback)) ||
      (complex && m < n)) {
    // BDSQR SCAL/SWAP consumes complete VT rows, including order one.
    // Complex GELQF/GEBD2 also conjugates the complete first row.
    row_length = n;
  }
  arithmetic.Sum({arithmetic.Product(row_length, lda), 1});
  if (operation == Operation::kGelss) {
    // Pinned BDSQR replaced MAXITR*N**2 with MAXITDIVN=6*N. Its
    // ITER+M-LL intermediate is bounded by 3*N, also covered here.
    arithmetic.Product(k, 6);
  }
}

// Actual SLASDT on the attested runtime first creates a 26-row leaf at
// 212992, crossing LALSD's U/V slices reserved for SMLSIZ=25. Any numerical
// split can produce that subproblem. Exhaustive source-expression checks
// below it and actual transition-neighborhood trees are retained externally.
// This is an execution-only gate: actual queries and all-zero A are safe.
template <typename Real>
Status DivideTreeAdmission(extent_t k) {
  if (k < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if constexpr (std::is_same_v<Real, float>) {
    if (k >= 212992) {
      return Status(ErrorCode::kUnsupported);
    }
  }
  return Status::Ok();
}

template <typename Real>
Result<Counts> QueryCounts(Operation operation, extent_t m, extent_t n,
                           extent_t nrhs, extent_t lda, bool complex,
                           extent_t limit) {
  if (!internal_lapack_least_squares::SupportedLimit(limit) ||
      (operation != Operation::kGelss && operation != Operation::kGelsd)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (m < 0 || n < 0 || nrhs < 0 || lda < std::max<extent_t>(1, m) ||
      m > limit || n > limit || nrhs > limit || lda > limit) {
    return Status(ErrorCode::kOverflow);
  }
  Arithmetic arithmetic(limit);
  const extent_t k = std::min(m, n);
  const extent_t crossover = arithmetic.Crossover(k);
  Counts counts =
      operation == Operation::kGelss
          ? GelssCounts<Real>(m, n, nrhs, complex, crossover, arithmetic)
          : GelsdCounts<Real>(m, n, nrhs, complex, crossover, arithmetic);
  ExecutionCounts<Real>(operation, m, n, nrhs, lda, complex, crossover, counts,
                        arithmetic);
  if (!arithmetic.valid()) {
    return Status(ErrorCode::kOverflow);
  }
  return counts;
}

}  // namespace asc::internal_lapack_svd_least_squares

#endif  // ASC_DENSE_LAPACK_INTERNAL_SVD_LEAST_SQUARES_COUNTS_H_
