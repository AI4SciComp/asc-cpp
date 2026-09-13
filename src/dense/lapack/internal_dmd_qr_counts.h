#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_QR_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_QR_COUNTS_H_

#include <algorithm>
#include <type_traits>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_dmd_execution_counts.h"
#include "internal_dmd_query_counts.h"
#include "internal_rank_revealing_counts.h"

namespace asc::internal_dmd_qr_counts {
// Source query encodings are observations, distinct from sufficient storage.
struct Counts {
  extent_t scalar;
  extent_t preferred;
  extent_t real;
  extent_t integer;
  extent_t source_scalar;
  extent_t source_preferred;
  extent_t source_real;
  extent_t query_scalar;
  extent_t query_preferred;
  extent_t query_real;
};
template <typename Real>
bool NestedVectors(char vectors, bool exact, bool complex) {
  // Preserve SGEDMDQ's pinned omission, whose mathematical gate fails.
  return vectors == 'V' || vectors == 'F' || exact ||
         (vectors == 'Q' && (complex || !std::is_same_v<Real, float>));
}
template <typename Real>
Result<Counts> Query(extent_t m, extent_t n, int svd, char vectors, bool exact,
                     bool orthogonal, bool complex, extent_t limit) {
  if (m < 0 || n < 0 || m < n || svd < 1 || svd > 4 ||
      (vectors != 'N' && vectors != 'V' && vectors != 'F' && vectors != 'Q') ||
      !internal_lapack_rank_revealing::SupportedLimit(limit)) {
    return Status(ErrorCode::kInvalidArgument);
  }
  if (n <= 1) {
    // The public route completes locally. Native complex void queries do not
    // initialize both complex query slots, so no native-value claim is made.
    return Counts{0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  }
  const auto nested = internal_dmd_counts::Query<Real>(
      n, n - 1, svd, NestedVectors<Real>(vectors, exact, complex), complex,
      limit);
  if (!nested.ok()) {
    return nested.status();
  }
  internal_dmd_counts::Arithmetic a(limit);
  a.Sum({m, 1});  // Native validation evaluates M+1 before other work.
  const auto qr = a.Round<Real>(a.Product(n, 32));
  auto minimum =
      std::max(a.Product(n, 2), a.Sum({n, nested->minimum.query_scalar}));
  auto preferred =
      std::max(a.Sum({n, qr}), a.Sum({n, nested->query_preferred}));
  auto sufficient = std::max(minimum, a.Sum({n, nested->minimum.scalar}));
  auto sufficient_preferred =
      std::max(preferred, a.Sum({n, nested->capacity_preferred}));
  const auto prefix = complex ? n : a.Sum({n, n - 1});
  if (vectors == 'V' || vectors == 'F' || orthogonal) {
    minimum = std::max(minimum, a.Sum({prefix, n}));
    sufficient = std::max(sufficient, minimum);
  }
  if (vectors == 'V' || vectors == 'F') {
    const auto apply = a.Round<Real>(a.Sum({a.Product(n, 32), 4160}));
    preferred = std::max(preferred, a.Sum({prefix, apply}));
  }
  if (orthogonal) {
    preferred = std::max(preferred, a.Sum({prefix, qr}));
  }
  const auto real =
      complex ? std::max<extent_t>(2, nested->minimum.query_real) : 0;
  const auto query_min = a.Round<Real>(minimum, false);
  const auto query_preferred = a.Round<Real>(preferred, false);
  const auto query_real = a.Round<Real>(real, false);
  sufficient = std::max(sufficient, query_min);
  sufficient_preferred =
      std::max({sufficient_preferred, preferred, query_preferred, sufficient});
  return a.valid() ? Result<Counts>(Counts{
                         sufficient, sufficient_preferred,
                         std::max({real, query_real, nested->minimum.real}),
                         nested->minimum.integer, minimum, preferred, real,
                         query_min, query_preferred, query_real})
                   : Result<Counts>(Status(ErrorCode::kOverflow));
}
template <typename Real>
Status ExecutionBounds(extent_t m, extent_t n, int svd, char vectors,
                       bool exact, bool orthogonal, bool complex,
                       extent_t limit) {
  const auto query =
      Query<Real>(m, n, svd, vectors, exact, orthogonal, complex, limit);
  if (!query.ok()) {
    return query.status();
  }
  if (n <= 1) {
    return Status::Ok();
  }
  auto nested = internal_dmd_counts::ExecutionBounds<Real>(
      n, n - 1, svd, NestedVectors<Real>(vectors, exact, complex), complex,
      limit);
  if (!nested.ok()) {
    return nested;
  }
  internal_dmd_counts::Arithmetic a(limit);
  a.Sum({a.Product(m, n), 1});
  a.Sum({m, 32});
  a.Sum({n, 32});
  a.Sum({query->preferred, 1});
  a.Sum({query->real, 1});
  a.Sum({query->integer, 1});
  if (vectors == 'V' || vectors == 'F') {
    a.Round<Real>(a.Sum({a.Product(n, 32), 4160}));
  }
  return a.valid() ? Status::Ok() : Status(ErrorCode::kOverflow);
}
}  // namespace asc::internal_dmd_qr_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_QR_COUNTS_H_
