#ifndef ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_CONDITION_H_
/** @file
 * @brief Explicit reciprocal-condition estimation from nominal PT factors.
 *
 * Uses the existing real-D/real-or-complex-E factor view, not square-root
 * Cholesky factors. Upper/lower orientations have the same Hermitian one-norm
 * condition under the documented conjugate conversion. The caller supplies
 * the original matrix one-norm; this API cannot certify that norm's provenance.
 *
 * Queries read metadata and scalar norm only, with no foreign call or array
 * numeric reads. Active calls require finite positive D and finite E, checked
 * after workspace admission. Caller RCOND is staged without reading its old
 * value. Preflight/provider defects preserve it; nonfinite returned estimates
 * publish unchanged with an accuracy warning. No clamping, scaling, alternate
 * provider or numerical fallback occurs. A finite zero is a raw estimate, not
 * proof of singularity. All native INFO values use the actual integer ABI.
 *
 * Execution enters PTCON even for N=0 or ANORM=0, retaining its native INFO0
 * and RCOND1/0 quick-return behavior; those paths read no factor values and
 * require no caller workspace. Other calls use N live real objects in kReal.
 * Calls allocate, transfer and synchronize nothing. Work/storage are O(N).
 * Independent buffers/workspaces/reports/contexts and immutable factor/plan
 * reuse are reentrant. Factor_family remains absent for these LDL^H factors.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
namespace asc {
/** @brief Plans SPTCON without reading factors or old caller output.
 * @param provider Checked explicit host provider matching the factor identity.
 * @param factor Nominal PT factors in either documented physical orientation.
 * @param original_norm Finite nonnegative one-norm of the represented matrix.
 * @param reciprocal_condition Disjoint live host output object, unread here.
 * @return Matching fixed workspace plan or structural/norm/overflow failure.
 * Plan identity binds scalar, order, orientation, provider and whether the norm
 * is zero; nonzero norm values and buffer addresses may change on plan reuse.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    float original_norm, const float& reciprocal_condition);
/** @brief Executes SPTCON with staged diagnostic publication.
 * @param provider Same provider identity used by the query and factor view.
 * @param factor Immutable factors; active numerical validation requires finite
 * positive D and finite E, without a hidden factorization or dense conversion.
 * @param original_norm Original Hermitian matrix one-norm, finite/nonnegative.
 * @param reciprocal_condition Receives the raw nonnegative estimate after
 * valid native return. Old values are never read; provider defects preserve it.
 * @param plan Matching metadata identity and fixed workspace requirements.
 * @param workspace N live float objects in kReal for active estimation; no
 * workspace for native N=0/zero-norm quick returns. Disjoint from all operands.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual INFO, entry, output validity and invalid-factor index/warning.
 * @return Success for finite nonnegative output; kNumerical for invalid active
 * factors or published nonfinite output; preflight errors preserve numeric
 * storage. Negative/missing estimates or any nonzero/unwritten/partial INFO
 * are provider defects, with caller output unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ptcon(const ReferenceLapackProvider& provider,
      ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
      float original_norm, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans DPTCON without reading factors or old caller output.
 * @param provider Checked explicit host provider matching the factor identity.
 * @param factor Nominal PT factors in either documented physical orientation.
 * @param original_norm Finite nonnegative one-norm of the represented matrix.
 * @param reciprocal_condition Disjoint live host output object, unread here.
 * @return Matching fixed workspace plan or structural/norm/overflow failure.
 * Plan identity binds scalar, order, orientation, provider and whether the norm
 * is zero; nonzero norm values and buffer addresses may change on plan reuse.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    double original_norm, const double& reciprocal_condition);
/** @brief Executes DPTCON with staged diagnostic publication.
 * @param provider Same provider identity used by the query and factor view.
 * @param factor Immutable factors; active numerical validation requires finite
 * positive D and finite E, without a hidden factorization or dense conversion.
 * @param original_norm Original Hermitian matrix one-norm, finite/nonnegative.
 * @param reciprocal_condition Receives the raw nonnegative estimate after
 * valid native return. Old values are never read; provider defects preserve it.
 * @param plan Matching metadata identity and fixed workspace requirements.
 * @param workspace N live double objects in kReal for active estimation; no
 * workspace for native N=0/zero-norm quick returns. Disjoint from all operands.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual INFO, entry, output validity and invalid-factor index/warning.
 * @return Success for finite nonnegative output; kNumerical for invalid active
 * factors or published nonfinite output; preflight errors preserve numeric
 * storage. Negative/missing estimates or any nonzero/unwritten/partial INFO
 * are provider defects, with caller output unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ptcon(const ReferenceLapackProvider& provider,
      ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
      double original_norm, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Plans CPTCON without reading factors or old caller output.
 * @param provider Checked explicit host provider matching the factor identity.
 * @param factor Nominal PT factors in either documented physical orientation.
 * @param original_norm Finite nonnegative one-norm of the represented matrix.
 * @param reciprocal_condition Disjoint live host output object, unread here.
 * @return Matching fixed workspace plan or structural/norm/overflow failure.
 * Plan identity binds scalar, order, orientation, provider and whether the norm
 * is zero; nonzero norm values and buffer addresses may change on plan reuse.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    float original_norm, const float& reciprocal_condition);
/** @brief Executes CPTCON with staged diagnostic publication.
 * @param provider Same provider identity used by the query and factor view.
 * @param factor Immutable factors; active numerical validation requires finite
 * positive D and finite E, without a hidden factorization or dense conversion.
 * @param original_norm Original Hermitian matrix one-norm, finite/nonnegative.
 * @param reciprocal_condition Receives the raw nonnegative estimate after
 * valid native return. Old values are never read; provider defects preserve it.
 * @param plan Matching metadata identity and fixed workspace requirements.
 * @param workspace N live float objects in kReal for active estimation; no
 * workspace for native N=0/zero-norm quick returns. Disjoint from all operands.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual INFO, entry, output validity and invalid-factor index/warning.
 * @return Success for finite nonnegative output; kNumerical for invalid active
 * factors or published nonfinite output; preflight errors preserve numeric
 * storage. Negative/missing estimates or any nonzero/unwritten/partial INFO
 * are provider defects, with caller output unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptcon(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    float original_norm, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Plans ZPTCON without reading factors or old caller output.
 * @param provider Checked explicit host provider matching the factor identity.
 * @param factor Nominal PT factors in either documented physical orientation.
 * @param original_norm Finite nonnegative one-norm of the represented matrix.
 * @param reciprocal_condition Disjoint live host output object, unread here.
 * @return Matching fixed workspace plan or structural/norm/overflow failure.
 * Plan identity binds scalar, order, orientation, provider and whether the norm
 * is zero; nonzero norm values and buffer addresses may change on plan reuse.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtconWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    double original_norm, const double& reciprocal_condition);
/** @brief Executes ZPTCON with staged diagnostic publication.
 * @param provider Same provider identity used by the query and factor view.
 * @param factor Immutable factors; active numerical validation requires finite
 * positive D and finite E, without a hidden factorization or dense conversion.
 * @param original_norm Original Hermitian matrix one-norm, finite/nonnegative.
 * @param reciprocal_condition Receives the raw nonnegative estimate after
 * valid native return. Old values are never read; provider defects preserve it.
 * @param plan Matching metadata identity and fixed workspace requirements.
 * @param workspace N live double objects in kReal for active estimation; no
 * workspace for native N=0/zero-norm quick returns. Disjoint from all operands.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual INFO, entry, output validity and invalid-factor index/warning.
 * @return Success for finite nonnegative output; kNumerical for invalid active
 * factors or published nonfinite output; preflight errors preserve numeric
 * storage. Negative/missing estimates or any nonzero/unwritten/partial INFO
 * are provider defects, with caller output unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptcon(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    double original_norm, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_CONDITION_H_
