#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_CONDITION_H_
/** @file
 * @brief Exact Reference-LAPACK condition estimates from expanded band LU.
 *
 * GBCON uses immutable column-major expanded GB factors with diagonal KL+KU,
 * matching nominal raw signed pivots, and the caller's finite nonnegative
 * norm of the original matrix. The caller preserves common factor/pivot
 * provenance and lifetimes; no successful factor tag is inferred. Every pivot
 * is checked against [j+1,min(n,j+kl+1)] before scratch changes. Singular
 * factors are admitted: the native scaled estimator can return RCOND zero.
 *
 * All full factor backing, pivots, output, metadata and nonempty scratch are
 * live, host/pinned-host accessible and pairwise disjoint. All detected
 * metadata aliases leave report untouched; otherwise report resets before
 * preflight. Structural rejection leaves numerical and workspace bytes
 * unchanged and makes no foreign call. Queries read no factor/pivot values.
 * Plans bind routine, scalar, dimensions, bands, physical stride, norm mode
 * and provider. Values and addresses are revalidated per execution.
 *
 * Real WORK needs 3*n scalar entries; complex WORK needs 2*n scalar and n
 * underlying-real entries. Native-width integer storage holds n pivots plus
 * n simultaneously live real estimator entries (only n pivots for complex).
 * No index_t reinterpretation, allocation, transfer, fallback, handler change
 * or synchronization occurs. Empty calls still execute pinned GBCON and
 * return RCOND one; nonempty ANORM zero returns RCOND zero. The estimate is
 * source arithmetic, not an exact inverse norm or blanket finite guarantee.
 * Raw INFO is mandatory; negative, impossible or unwritten INFO is an
 * unusable provider defect. No factor family is certified in the report.
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
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_condition.h"
namespace asc {
/** @brief Queries exact single real GBCON caller scratch.
 * @param provider Explicit serial reference provider.
 * @param norm One- or infinity-norm estimator selection.
 * @param factors Immutable expanded square band factor backing.
 * @param pivots Matching nominal signed raw swaps, exactly n contiguous
 * entries.
 * @param original_norm Finite nonnegative selected norm of original A.
 * @param reciprocal_condition Unchanged disjoint live output scalar.
 * @return Metadata-only formula plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    float original_norm, const float& reciprocal_condition);
/** @brief Executes pinned single real scaled band inverse-norm estimate.
 * @param provider Explicit reference provider matching the plan.
 * @param norm Selected original and inverse matrix norm.
 * @param factors Immutable expanded GB factors, including complete backing.
 * @param pivots Matching signed band swaps, all values checked before writes.
 * @param original_norm Finite nonnegative original matrix norm.
 * @param reciprocal_condition Raw native RCOND, including singular zero.
 * @param plan Unmodified matching formula plan.
 * @param workspace Exact-capacity caller scalar/real/native-integer storage.
 * @param report Mandatory disjoint raw INFO and outcome record.
 * @return OK on native INFO zero, structural error, or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
      float original_norm, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Queries exact double real GBCON caller scratch.
 * @param provider Explicit serial reference provider.
 * @param norm One- or infinity-norm estimator selection.
 * @param factors Immutable expanded square band factor backing.
 * @param pivots Matching nominal signed raw swaps, exactly n contiguous
 * entries.
 * @param original_norm Finite nonnegative selected norm of original A.
 * @param reciprocal_condition Unchanged disjoint live output scalar.
 * @return Metadata-only formula plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    double original_norm, const double& reciprocal_condition);
/** @brief Executes pinned double real scaled band inverse-norm estimate.
 * @param provider Explicit reference provider matching the plan.
 * @param norm Selected original and inverse matrix norm.
 * @param factors Immutable expanded GB factors, including complete backing.
 * @param pivots Matching signed band swaps, all values checked before writes.
 * @param original_norm Finite nonnegative original matrix norm.
 * @param reciprocal_condition Raw native RCOND, including singular zero.
 * @param plan Unmodified matching formula plan.
 * @param workspace Exact-capacity caller scalar/real/native-integer storage.
 * @param report Mandatory disjoint raw INFO and outcome record.
 * @return OK on native INFO zero, structural error, or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
      double original_norm, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Queries exact single complex GBCON caller scratch.
 * @param provider Explicit serial reference provider.
 * @param norm One- or infinity-norm estimator selection.
 * @param factors Immutable expanded square band factor backing.
 * @param pivots Matching nominal signed raw swaps, exactly n contiguous
 * entries.
 * @param original_norm Finite nonnegative selected norm of original A.
 * @param reciprocal_condition Unchanged disjoint live output scalar.
 * @return Metadata-only formula plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots, float original_norm,
    const float& reciprocal_condition);
/** @brief Executes pinned single complex scaled band inverse-norm estimate.
 * @param provider Explicit reference provider matching the plan.
 * @param norm Selected original and inverse matrix norm.
 * @param factors Immutable expanded GB factors, including complete backing.
 * @param pivots Matching signed band swaps, all values checked before writes.
 * @param original_norm Finite nonnegative original matrix norm.
 * @param reciprocal_condition Raw native RCOND, including singular zero.
 * @param plan Unmodified matching formula plan.
 * @param workspace Exact-capacity caller scalar/real/native-integer storage.
 * @param report Mandatory disjoint raw INFO and outcome record.
 * @return OK on native INFO zero, structural error, or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackLuBandView<const std::complex<float>> factors,
      ReferenceLuBandPivotView pivots, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries exact double complex GBCON caller scratch.
 * @param provider Explicit serial reference provider.
 * @param norm One- or infinity-norm estimator selection.
 * @param factors Immutable expanded square band factor backing.
 * @param pivots Matching nominal signed raw swaps, exactly n contiguous
 * entries.
 * @param original_norm Finite nonnegative selected norm of original A.
 * @param reciprocal_condition Unchanged disjoint live output scalar.
 * @return Metadata-only formula plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots, double original_norm,
    const double& reciprocal_condition);
/** @brief Executes pinned double complex scaled band inverse-norm estimate.
 * @param provider Explicit reference provider matching the plan.
 * @param norm Selected original and inverse matrix norm.
 * @param factors Immutable expanded GB factors, including complete backing.
 * @param pivots Matching signed band swaps, all values checked before writes.
 * @param original_norm Finite nonnegative original matrix norm.
 * @param reciprocal_condition Raw native RCOND, including singular zero.
 * @param plan Unmodified matching formula plan.
 * @param workspace Exact-capacity caller scalar/real/native-integer storage.
 * @param report Mandatory disjoint raw INFO and outcome record.
 * @return OK on native INFO zero, structural error, or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      LapackLuBandView<const std::complex<double>> factors,
      ReferenceLuBandPivotView pivots, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_CONDITION_H_
