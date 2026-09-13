#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_CONDITION_H_

/** @file
 * @brief Explicit reference one-norm reciprocal-condition estimation from
 * Cholesky.
 *
 * POCON consumes the selected triangular factor from POTRF and a finite
 * nonnegative one-norm of the original symmetric/Hermitian matrix. It estimates
 * RCOND; it is not an exact condition number or a forward-accuracy certificate.
 * Complex matrix norms use Euclidean modulus. Factor diagonals are triangular
 * coefficients, so their complex imaginary components are not ignored.
 *
 * Formula-only queries validate metadata and the supplied norm without reading
 * factor/output entries or calling the provider. Active real calls require
 * 3*n live scalar WORK entries and n ABI-width kInteger entries. Active complex
 * calls require 2*n live scalar WORK entries and n underlying-real kReal
 * entries. Both paths check the source LACN2 expression 3*n. Row-major needs
 * n*n additional live scalar kLayoutConversion entries; only the selected
 * triangle is packed. Column-major remains direct. No hidden allocation,
 * transfer, synchronization, automatic provider selection or packing occurs.
 *
 * Plans bind routine/type, triangle, all dimensions, original ASC stride and
 * actual foreign stride, layout, zero-norm branch and provider build/ABI.
 * Storage must be accessible to the explicit serial provider. A and RCOND
 * must be disjoint from each other, scratch and live metadata. Caller lifetimes
 * cover the call; independent concurrent operations use disjoint writable
 * storage and reports. Raw factors must come from the selected provider and
 * match the original norm; raw storage carries no fabricated provenance.
 *
 * Structural failures leave numerical output/scratch unchanged, with absent
 * INFO and called_provider false. The report is reset before preflight except
 * that unsafe metadata aliases leave it untouched. For n=0, RCOND=1; for
 * nonempty zero original norm, RCOND=0. These are successful noncalls and
 * require no scratch or factor read.
 *
 * POCON has no documented positive INFO. Any nonzero INFO is a provider defect
 * with exact raw INFO retained and unusable output. INFO=0 with finite
 * nonnegative RCOND is complete; zero can mean singularity, scaling underflow
 * or extreme ill-conditioning and does not fabricate a pivot diagnostic.
 * Nonfinite RCOND produces kNumerical/kAccuracyWarning and retained partial
 * output; a negative RCOND is a provider defect. Factor values are not scanned
 * for finiteness or a positive-diagonal certificate. A and padding are never
 * mutated, including on provider failure.
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

/** @brief Queries single real POCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the Cholesky factor.
 * @param factors Immutable square raw factor in either layout; entries unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single real reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable square raw factor; ignored triangle is not read.
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
Pocon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const float> factors, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real POCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the Cholesky factor.
 * @param factors Immutable square raw factor in either layout; entries unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, double original_norm,
    const double& reciprocal_condition);

/** @brief Estimates double real reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable square raw factor; ignored triangle is not read.
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
Pocon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const double> factors, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex POCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the Cholesky factor.
 * @param factors Immutable square raw factor in either layout; entries unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single complex reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable square raw factor; ignored triangle is not read.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar, real/integer and packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, numerical estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Pocon(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors, float original_norm,
    float& reciprocal_condition, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex POCON workspace without a foreign call.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Stored upper/lower triangle of the Cholesky factor.
 * @param factors Immutable square raw factor in either layout; entries unread.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host RCOND output, unread.
 * @return Complete metadata-bound formula plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition);

/** @brief Estimates double complex reciprocal condition from a Cholesky factor.
 * @param provider Explicit checked reference provider; no fallback.
 * @param triangle Stored upper/lower factor triangle.
 * @param factors Immutable square raw factor; ignored triangle is not read.
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
Pocon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const std::complex<double>> factors,
      double original_norm, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_CONDITION_H_
