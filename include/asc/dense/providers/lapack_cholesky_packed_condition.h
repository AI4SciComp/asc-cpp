#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_CONDITION_H_

/** @file
 * @brief Explicit pinned reciprocal-condition estimates from packed Cholesky.
 *
 * PPCON consumes an immutable ordinary packed upper/lower Cholesky factor and
 * the finite nonnegative one-norm of the original symmetric/Hermitian matrix.
 * Complex original norms use Euclidean modulus. Complex factor diagonals are
 * triangular coefficients; their imaginary components are read. Raw factors
 * must match the selected provider and original norm; no provenance or
 * positive-definiteness certificate is inferred from their storage.
 *
 * Formula-only queries validate metadata and the supplied norm without reading
 * factor/output entries or entering the provider. Plans bind routine/scalar,
 * provider/build/INTEGER identity, order, triangle, layout and active norm
 * branch. Different positive finite norms may reuse the same plan. N=0 returns
 * RCOND=1 locally; positive order with zero norm returns RCOND=0 locally. Both
 * local cases require zero scratch, do not read factor/scratch entries and
 * report absent native INFO. Invalid flags/norm/metadata still fail.
 *
 * Active real calls require 3*N scalar WORK and N ABI-width kInteger entries;
 * complex calls require 2*N scalar WORK and N underlying-real kReal entries.
 * Source admission protects actual packed N*(N+1), terminal packed cursors,
 * 3*N and all native loop exits before numeric writes. Active row layout uses
 * p=N*(N+1)/2 additional live scalar kLayoutConversion entries, preserving
 * every stored coefficient while packing the original triangle. Column layout
 * is direct. No densification, input scan, implicit scaling, fallback,
 * allocation, transfer, synchronization or global handler is introduced.
 *
 * A, RCOND, workspace and live provider/plan/report metadata must be disjoint
 * under existing accessible host/pinned-host and containing-object contracts.
 * All structural and stale-plan failures preserve numeric/scratch bytes and
 * avoid provider entry. Unsafe metadata aliases preserve report; other
 * preflight resets it. Calls are synchronous; independent concurrent calls
 * use disjoint writable storage/reports and exclude conflicting access.
 *
 * Immediately before native entry, RCOND is initialized to quiet NaN and the
 * full native INFO object to its signed minimum. Any nonzero, missing or
 * partial INFO is a provider defect with exact raw INFO and unusable direct
 * outputs retained. Negative RCOND is a provider defect. INFO0 with nonfinite
 * RCOND is kNumerical/kAccuracyWarning with retained partial output; finite
 * nonnegative RCOND is complete. Zero may reflect singularity, underflow or
 * extreme ill-conditioning and gives no fabricated pivot diagnosis. RCOND is
 * an estimate, not an exact condition number or a forward-error certificate.
 * A and all its backing guards remain unchanged on every outcome. Link the
 * optional ASC::dense_lapack target explicitly; native coverage is separate.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real PPCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the ordinary packed factor.
 * @param factors Immutable ordinary packed raw factor in either layout; entries
 * unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> factors, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single real reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable ordinary packed raw factor; every stored coefficient
 * is input.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar, real/integer and packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, numerical estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const float> factors, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real PPCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the ordinary packed factor.
 * @param factors Immutable ordinary packed raw factor in either layout; entries
 * unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, double original_norm,
    const double& reciprocal_condition);

/** @brief Estimates double real reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable ordinary packed raw factor; every stored coefficient
 * is input.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar, real/integer and packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, numerical estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const double> factors, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex PPCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the ordinary packed factor.
 * @param factors Immutable ordinary packed raw factor in either layout; entries
 * unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    float original_norm, const float& reciprocal_condition);

/** @brief Estimates single complex reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable ordinary packed raw factor; every stored coefficient
 * is input.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar, real/integer and packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, numerical estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> factors,
      float original_norm, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex PPCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the ordinary packed factor.
 * @param factors Immutable ordinary packed raw factor in either layout; entries
 * unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition);

/** @brief Estimates double complex reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable ordinary packed raw factor; every stored coefficient
 * is input.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar, real/integer and packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, numerical estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> factors,
      double original_norm, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_CONDITION_H_
