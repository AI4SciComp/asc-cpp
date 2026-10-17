#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_DRIVER_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_DRIVER_COUNTS_H_
#include <algorithm>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_aasen_counts.h"
#include "internal_indefinite_aasen_solve_counts.h"
namespace asc::internal_indefinite_aasen_driver_counts {
inline Status Driver(extent_t n, extent_t nrhs, extent_t lda, extent_t ldb,
                     extent_t limit) {
  auto solve =
      internal_indefinite_aasen_solve_counts::Solve(n, nrhs, lda, ldb, limit);
  if (!solve.ok()) {
    return solve;
  }
  // Native driver queries and validates both callees even with zero RHS.
  if (n > limit / 3) {
    return Status(ErrorCode::kOverflow);
  }
  return internal_indefinite_aasen_counts::Factor(n, lda, limit);
}
template <typename Real>
Result<extent_t> Preferred(extent_t n, bool complex_symmetric, extent_t limit) {
  const auto factor = internal_indefinite_aasen_counts::Preferred<Real>(
      n, complex_symmetric, limit);
  if (!factor.ok()) {
    return factor.status();
  }
  if (n == 0) {
    return extent_t{0};
  }
  const auto solve =
      internal_indefinite_aasen_solve_counts::Preferred<Real>(n, limit);
  if (!solve.ok()) {
    return solve.status();
  }
  return std::max({2 * n, 3 * n - 2, *factor, *solve});
}
}  // namespace asc::internal_indefinite_aasen_driver_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_DRIVER_COUNTS_H_
