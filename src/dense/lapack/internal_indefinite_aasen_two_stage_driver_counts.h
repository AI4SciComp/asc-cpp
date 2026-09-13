#ifndef ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_DRIVER_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_DRIVER_COUNTS_H_
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_indefinite_aasen_two_stage_counts.h"
#include "internal_indefinite_aasen_two_stage_solve_counts.h"
#include "internal_indefinite_counts.h"
namespace asc::internal_indefinite_aasen_two_stage_driver_counts {
inline Status Driver(extent_t n, extent_t nrhs, extent_t lda, extent_t ltb,
                     extent_t ldb, extent_t limit) {
  if (!internal_indefinite_counts::Supported(limit) || n < 0 || nrhs < 0 ||
      lda < 1 || ldb < 1 || ltb < 0) {
    return Status(ErrorCode::kInvalidArgument);
  }
  // Every native invocation first queries both factor capacities. The TB
  // recommendation forms (3*192+1)*N even when ordinary LTB is only 4*N.
  if (n > limit / 577 || nrhs > limit || ldb > limit) {
    return Status(ErrorCode::kOverflow);
  }
  if (n > 0 && ldb < n) {
    return Status(ErrorCode::kShape);
  }
  auto factor =
      internal_indefinite_aasen_two_stage_counts::Factor(n, lda, ltb, limit);
  if (!factor.ok()) {
    return factor;
  }
  // NRHS=0 still factors A. Only the subsequent solve has an empty return.
  return internal_indefinite_aasen_two_stage_solve_counts::Solve(
      n, nrhs, lda, ltb, ldb, limit);
}
template <typename Real>
Result<extent_t> Preferred(extent_t n, extent_t limit) {
  // This also guards the native driver's INT(WORK(1)) conversion after its
  // internal factor query, retaining the pinned scalar rounding behavior.
  return internal_indefinite_aasen_two_stage_counts::Preferred<Real>(n, limit);
}
}  // namespace asc::internal_indefinite_aasen_two_stage_driver_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_INDEFINITE_AASEN_TWO_STAGE_DRIVER_COUNTS_H_
