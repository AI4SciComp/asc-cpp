#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_CONDITION_H_

/** @file
 * @brief Explicit reference reciprocal-condition estimation for band factors.
 *
 * PBCON consumes raw upper U with A=U^H U or lower L with A=L L^H,
 * using transpose for real. The caller supplies the finite nonnegative
 * original matrix one-norm. Complex norms use Euclidean scalar modulus.
 * The descriptor certifies storage only, not successful factor provenance.
 * All raw complex factor components, including diagonals, are preserved;
 * there is no factor-diagonal predicate, normalization, or tolerance.
 *
 * For n>0 and ANORM>0, real WORK requires 3*n scalar entries and n private
 * ABI-width kInteger entries; complex WORK requires 2*n scalar and n
 * underlying-real kReal entries. Row-major additionally requires n*(kd+1)
 * live scalar kLayoutConversion entries. This is selected band packing, never
 * dense expansion, hidden allocation, transfer, synchronization or fallback.
 * Execution begins trivial foreign-integer lifetimes in the checked byte
 * region; callers provide live scalar/real objects. All live operands,
 * workspace regions and metadata are disjoint.
 *
 * If n=0 or ANORM=0, no numerical workspace or packing is required and no
 * factor value is read. An actual upstream call returns RCOND=1 for n=0,
 * otherwise zero for ANORM=0. The plan binds this active/inactive distinction
 * along with dimensions, actual foreign leading dimension, original ASC
 * stride/layout, triangle and provider identity. These are checked formula
 * plans, not foreign LWORK queries.
 *
 * Preflight failures precede mutation/foreign entry and allocate nothing;
 * report reset is withheld only when metadata aliases prevent a safe reset.
 * Every nonempty supplied region, including unused roles, must be admitted
 * by the explicit provider context; zero-byte compatibility is retained.
 * INFO=0 is a raw completed estimate, not an exact condition calculation or
 * finite-factor promise. Zero can mean singularity, underflow, or extreme
 * conditioning and does not invent a failed pivot. A negative/nonfinite
 * RCOND triggers kNumerical with kAccuracyWarning/documented-partial report
 * while preserving actual INFO=0 and the raw value. Negative or impossible
 * INFO is kProvider/unusable. Immutable factors/padding remain unchanged.
 * The pinned complex lower LATBS path has a known scaled KD=1 estimate
 * limitation: finite INFO=0 can disagree with the independent condition
 * of a small dyadic matrix. This adapter preserves the raw estimate and
 * never substitutes upper storage, another provider, or a recomputation.
 * The corresponding required mathematical test remains an unmet gate.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Computes checked PBCON numerical and band-packing capacities.
 * @param provider Explicit checked selection; query does not call upstream.
 * @param factors Immutable selected raw Cholesky band in either layout.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host output object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> factors, float original_norm,
    const float& reciprocal_condition);
/** @brief Estimates reciprocal one-norm conditioning from a raw band factor.
 * @param provider Explicit checked reference provider and context.
 * @param factors Immutable raw factor; provenance is the caller's obligation.
 * @param original_norm Finite nonnegative original matrix one-norm.
 * @param reciprocal_condition Receives the raw real RCOND estimate.
 * @param plan Unmodified matching formula plan, including active-route state.
 * @param workspace Caller-owned disjoint live numerical and packing storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, structural failure, kNumerical quality warning, or provider
 * defect; see the complete file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbcon(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const float> factors, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked PBCON numerical and band-packing capacities.
 * @param provider Explicit checked selection; query does not call upstream.
 * @param factors Immutable selected raw Cholesky band in either layout.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host output object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> factors, double original_norm,
    const double& reciprocal_condition);
/** @brief Estimates reciprocal one-norm conditioning from a raw band factor.
 * @param provider Explicit checked reference provider and context.
 * @param factors Immutable raw factor; provenance is the caller's obligation.
 * @param original_norm Finite nonnegative original matrix one-norm.
 * @param reciprocal_condition Receives the raw real RCOND estimate.
 * @param plan Unmodified matching formula plan, including active-route state.
 * @param workspace Caller-owned disjoint live numerical and packing storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, structural failure, kNumerical quality warning, or provider
 * defect; see the complete file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Pbcon(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> factors, double original_norm,
    double& reciprocal_condition, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked PBCON numerical and band-packing capacities.
 * @param provider Explicit checked selection; query does not call upstream.
 * @param factors Immutable selected raw Cholesky band in either layout.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host output object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    float original_norm, const float& reciprocal_condition);
/** @brief Estimates reciprocal one-norm conditioning from a raw band factor.
 * @param provider Explicit checked reference provider and context.
 * @param factors Immutable raw factor; provenance is the caller's obligation.
 * @param original_norm Finite nonnegative original matrix one-norm.
 * @param reciprocal_condition Receives the raw real RCOND estimate.
 * @param plan Unmodified matching formula plan, including active-route state.
 * @param workspace Caller-owned disjoint live numerical and packing storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, structural failure, kNumerical quality warning, or provider
 * defect; see the complete file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbcon(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<float>> factors,
      float original_norm, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes checked PBCON numerical and band-packing capacities.
 * @param provider Explicit checked selection; query does not call upstream.
 * @param factors Immutable selected raw Cholesky band in either layout.
 * @param original_norm Finite nonnegative one-norm of the original A.
 * @param reciprocal_condition Disjoint live host output object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbconWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition);
/** @brief Estimates reciprocal one-norm conditioning from a raw band factor.
 * @param provider Explicit checked reference provider and context.
 * @param factors Immutable raw factor; provenance is the caller's obligation.
 * @param original_norm Finite nonnegative original matrix one-norm.
 * @param reciprocal_condition Receives the raw real RCOND estimate.
 * @param plan Unmodified matching formula plan, including active-route state.
 * @param workspace Caller-owned disjoint live numerical and packing storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, structural failure, kNumerical quality warning, or provider
 * defect; see the complete file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbcon(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<double>> factors,
      double original_norm, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_CONDITION_H_
