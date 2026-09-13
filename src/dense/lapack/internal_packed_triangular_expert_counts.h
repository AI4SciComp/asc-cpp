#ifndef ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_EXPERT_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_EXPERT_COUNTS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "internal_packed_triangular_counts.h"
#include "internal_triangular_expert_counts.h"

namespace asc::internal_packed_triangular_expert_counts {

// Formula-only integer admission; descriptors and options are checked by the
// caller. TPCON uses nrhs=1, ldb=ldx=max(1,n); TPRFS binds actual RHS strides.
inline Status Active(extent_t n, extent_t nrhs, extent_t ldb, extent_t ldx,
                     extent_t limit) {
  Status linear = internal_triangular_expert_counts::Active(
      n, nrhs, std::max<extent_t>(1, n), ldb, ldx, limit);
  if (!linear.ok() || n == 0 || nrhs == 0) {
    return linear;
  }
  // LACN2 requires 3*N even for complex WORK. Both reverse-communication
  // orientations are reachable: LATPS/TPSV evaluate n*(n+1) before division.
  // The same product bound, together with 3*n, covers LANTP/TPRFS packed
  // cursors, lower KC+n before subtraction, and LATPS's terminal p+n+1.
  return internal_packed_triangular_counts::Inverse(
      n, DenseBlasTriangle::kLower, limit);
}

}  // namespace asc::internal_packed_triangular_expert_counts

#endif  // ASC_DENSE_LAPACK_INTERNAL_PACKED_TRIANGULAR_EXPERT_COUNTS_H_
