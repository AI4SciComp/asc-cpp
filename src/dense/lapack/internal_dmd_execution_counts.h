#ifndef ASC_DENSE_LAPACK_INTERNAL_DMD_EXECUTION_COUNTS_H_
#define ASC_DENSE_LAPACK_INTERNAL_DMD_EXECUTION_COUNTS_H_

#include <algorithm>

#include "asc/core/status.h"
#include "asc/core/types.h"
#include "internal_dmd_query_counts.h"
#include "internal_rank_revealing_counts.h"

namespace asc::internal_dmd_counts {

// Bounds for a provider call using contiguous column-major matrix staging.
// These checks do not inspect numerical inputs or claim convergence. Query
// encodings and sufficient storage capacities are deliberately kept separate.
template <typename Real>
Status ExecutionBounds(extent_t m, extent_t n, int svd, bool vectors,
                       bool complex, extent_t limit) {
  const auto query = Query<Real>(m, n, svd, vectors, complex, limit);
  if (!query.ok()) {
    return query.status();
  }
  if (n == 0) {
    return Status::Ok();
  }
  Arithmetic a(limit);
  const auto matrix = a.Product(m, n);
  const auto square = a.Product(n, n);
  // Full first-row BLAS cursors and source loops include terminal increments.
  a.Sum({matrix, 1});
  a.Sum({m, 32});
  a.Sum({n, 32});
  a.Sum({query->capacity_preferred, 1});
  a.Sum({query->minimum.real, 1});
  a.Sum({query->minimum.integer, 1});
  // GEBRD/QR panel arithmetic. The main GESVD tall path additionally tests
  // max(WRKBL,m*n+[n])+m*n even when the caller chose minimum workspace.
  // A square/near-square GESVD never evaluates those tall-path thresholds.
  a.Round<Real>(a.Product(a.Sum({m, n}), 32));
  if (svd == 1 && m >= a.Crossover(n)) {
    const auto block = a.Product(n, complex ? 66 : 67);
    a.Sum({matrix, std::max(block, a.Sum({matrix, complex ? 0 : n})), 1});
  }
  if (svd == 2) {
    // GESDD('O') tests its padded temporary threshold independently of the
    // optimal query. In the real route BDSPAC is3*n*n+4*n.
    a.Sum({matrix, a.Product(complex ? 1 : 4, square),
           a.Product(complex ? 3 : 7, n), 1});
  }
  const auto apply = a.Round<Real>(a.Sum({a.Product(n, 32), 4160}));
  a.Sum({n, apply});

  // GEEV may use any rank k in [1,n]. Its IPARMQ query is not monotone:
  // bounding the window by385 covers all ranks, including the180/182 drop.
  // GEHRD's actual preferred expression includes a fixed4160-entry T block.
  const auto eigen_window = std::max<extent_t>(n, 16865);
  a.Round<Real>(a.Sum({n, eigen_window}));
  a.Round<Real>(a.Sum({n, apply}));
  if (vectors) {
    a.Round<Real>(a.Sum({n, a.Round<Real>(a.Product(n, 129))}));
  }
  // LAHQR and LAQR0/4 each allow30*max(10,k) iterations, plus termination.
  a.Sum({a.Product(30, std::max<extent_t>(10, n)), 1});
  // BDSQR uses6*k, not the obsolete6*k*k limit. LASD0's explicit-vector
  // divide tree uses full n-by-n U/VT slices, unlike GELSD's small leaf slices.
  a.Sum({a.Product(n, 6), 1});

  if (svd == 3 || svd == 4) {
    const auto pivoted = internal_lapack_rank_revealing::Geqp3Counts<Real>(
        m, n, m, complex, limit);
    if (!pivoted.ok()) {
      return pivoted.status();
    }
    // GESVDQ executes GESVD('S','O',rank,n), including a wide subproblem.
    // Its query/temporary offsets are bounded by2*n*n+67*n+1 for all ranks.
    // GEJSV's selected Jacobi route has no larger square work offset.
    a.Round<Real>(a.Sum({a.Product(2, square), a.Product(n, 67), 1}));
    // Jacobi sweep pair counts multiply before dividing by two; tile end
    // indices can advance eight entries beyond the last incomplete tile.
    a.Product(n, n - 1);
    a.Sum({n, 8});
  }
  return a.valid() ? Status::Ok() : Status(ErrorCode::kOverflow);
}

}  // namespace asc::internal_dmd_counts
#endif  // ASC_DENSE_LAPACK_INTERNAL_DMD_EXECUTION_COUNTS_H_
