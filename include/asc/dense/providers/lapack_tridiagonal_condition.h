#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_CONDITION_H_

/** @file
 * @brief Explicit reference general-tridiagonal condition estimates.
 *
 * The lifetime, context, alias, report, allocation and plan contracts of
 * lapack_tridiagonal.h apply. No full matrix is formed and no global provider
 * selection occurs. All referenced arrays are disjoint contiguous CPU storage.
 * Queries read metadata and scalar options only; execution checks borrowed
 * adjacent pivot values after context admission. Raw factors and pivots must
 * originate together; validation cannot prove numerical provenance. No factor
 * report is fabricated, and report.factor_family remains absent.
 * GTCON accepts raw singular U: exact zero D yields RCOND=0 with raw INFO=0.
 * original_norm is finite, nonnegative, and the one/infinity norm of original A
 * (complex scalar moduli). An estimate is not a certificate or an inverse.
 * The fixed workspace is 2*n live scalars. kInteger starts with n provider
 * pivot integers; real types append a disjoint n-integer estimator array.
 * These two arrays remain live simultaneously; complex types need only n.
 * n=0 completes locally with RCOND=1 and no foreign INFO. Negative diagnostic
 * output is a provider defect; nonfinite output is preserved as an accuracy
 * warning. Structural failure leaves RCOND unchanged.
 * In the pinned source, a nonsingular singleton with diagonal equal to the
 * underlying real minimum normal value divided by eight returns RCOND=0:
 * the unscaled inverse estimate overflows although its exact condition is one.
 * ASC preserves this raw estimate; the required extreme-scaling mathematical
 * gate remains incomplete. Ordinary INFO=0 is not an accuracy certificate.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_lu_condition.h"
#include "asc/dense/providers/lapack_tridiagonal.h"

namespace asc {

/** @brief Checks single real GTCON metadata and fixed capacities.
 * @param provider Explicit checked reference provider, never called by the
 * query.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND object; query leaves it
 * unchanged.
 * @return A metadata-bound plan or structural failure, without numerical reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, float original_norm,
    const float& reciprocal_condition);
/** @brief Estimates single real reciprocal condition from raw LU.
 * @param provider Explicit checked reference provider with no fallback.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND output; raw estimate is
 * preserved.
 * @param plan Unmodified matching formula-query result.
 * @param workspace Caller-owned live scalar and separate integer regions.
 * @param report Mandatory failure-surviving report with actual raw INFO when
 * called.
 * @return OK for a complete estimate, structural/numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackTridiagonalLuStorage<const float> factors,
      ReferenceTridiagonalPivotView pivots, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double real GTCON metadata and fixed capacities.
 * @param provider Explicit checked reference provider, never called by the
 * query.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND object; query leaves it
 * unchanged.
 * @return A metadata-bound plan or structural failure, without numerical reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, double original_norm,
    const double& reciprocal_condition);
/** @brief Estimates double real reciprocal condition from raw LU.
 * @param provider Explicit checked reference provider with no fallback.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND output; raw estimate is
 * preserved.
 * @param plan Unmodified matching formula-query result.
 * @param workspace Caller-owned live scalar and separate integer regions.
 * @param report Mandatory failure-surviving report with actual raw INFO when
 * called.
 * @return OK for a complete estimate, structural/numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackTridiagonalLuStorage<const double> factors,
      ReferenceTridiagonalPivotView pivots, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks single complex GTCON metadata and fixed capacities.
 * @param provider Explicit checked reference provider, never called by the
 * query.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND object; query leaves it
 * unchanged.
 * @return A metadata-bound plan or structural failure, without numerical reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots, float original_norm,
    const float& reciprocal_condition);
/** @brief Estimates single complex reciprocal condition from raw LU.
 * @param provider Explicit checked reference provider with no fallback.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND output; raw estimate is
 * preserved.
 * @param plan Unmodified matching formula-query result.
 * @param workspace Caller-owned live scalar and separate integer regions.
 * @param report Mandatory failure-surviving report with actual raw INFO when
 * called.
 * @return OK for a complete estimate, structural/numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackTridiagonalLuStorage<const std::complex<float>> factors,
      ReferenceTridiagonalPivotView pivots, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double complex GTCON metadata and fixed capacities.
 * @param provider Explicit checked reference provider, never called by the
 * query.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND object; query leaves it
 * unchanged.
 * @return A metadata-bound plan or structural failure, without numerical reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots, double original_norm,
    const double& reciprocal_condition);
/** @brief Estimates double complex reciprocal condition from raw LU.
 * @param provider Explicit checked reference provider with no fallback.
 * @param norm One or infinity norm selection.
 * @param factors Immutable raw tridiagonal LU, including separate DU2.
 * @param pivots Same-factor nominal one-based adjacent pivots.
 * @param original_norm Finite nonnegative norm of the unfactored matrix.
 * @param reciprocal_condition Live disjoint host RCOND output; raw estimate is
 * preserved.
 * @param plan Unmodified matching formula-query result.
 * @param workspace Caller-owned live scalar and separate integer regions.
 * @param report Mandatory failure-surviving report with actual raw INFO when
 * called.
 * @return OK for a complete estimate, structural/numerical failure or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gtcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackTridiagonalLuStorage<const std::complex<double>> factors,
      ReferenceTridiagonalPivotView pivots, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_CONDITION_H_
