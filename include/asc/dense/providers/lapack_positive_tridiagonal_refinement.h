#ifndef ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_REFINEMENT_H_
/** @file
 * @brief Explicit refinement and estimates from nominal PT factors.
 *
 * Original E retains the existing descriptor's lower off-diagonal meaning.
 * Complex upper-factor calls explicitly pack its conjugate for native UPLO=U.
 * B and initial X are numeric inputs; old FERR/BERR are not. Queries and
 * preflight rejection inspect metadata only. Active execution validates
 * nominal finite positive D and finite E factors after workspace admission.
 * Original entries and initial solution are passed without a hidden rescale.
 *
 * X and estimates publish transactionally after full-width native INFO0 and
 * nonnegative written estimates. Provider defects preserve caller X/FERR/BERR;
 * nonfinite returned values publish unchanged with an accuracy warning. No
 * clamp, factorization, provider substitution or numerical fallback occurs.
 * N=0/NRHS=0 complete locally, writing zero estimates without input reads.
 *
 * Explicit workspace holds a contiguous X copy, row-major B packing when
 * needed, N-1 conjugated original E entries for complex upper factors,
 * real 2N or complex N plus underlying-real N native work, and two
 * NRHS-length real estimate arrays. The kScratch requirement counts NRHS
 * two-real sizing units: callers provide one live flat 2*NRHS real array,
 * first FERR then BERR. It is not a foreign LWORK count or an array of pairs.
 * Work is O(N*NRHS), bounded by the pinned five refinement steps; storage is
 * O(N*NRHS+N+NRHS). Operations allocate, transfer and synchronize nothing.
 * Independent buffers/workspaces/reports/contexts and immutable factor/plan
 * reuse are reentrant; shared mutable workspace is not supported.
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
#include "asc/dense/providers/lapack_positive_tridiagonal.h"
namespace asc {
/** @brief Plans SPTRFS using metadata only, without a foreign call.
 * @param provider Explicit accessible host provider matching the factor.
 * @param original Original matrix with its documented lower off-diagonal E.
 * @param factor Nominal immutable real-D/unit-bidiagonal-E PT factors.
 * @param rhs Original N-by-NRHS right-hand sides in either padded full layout.
 * @param solution Input estimate and output refinement, independently laid out.
 * @param forward_error Disjoint contiguous NRHS-length real output vector.
 * @param backward_error Disjoint contiguous NRHS-length real output vector.
 * @return Fixed matching plan or shape/access/alias/native-count/byte failure.
 * Buffer addresses and numeric values may change on immutable-plan reuse;
 * dimensions, physical orientations, strides, layouts and provider must match.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes SPTRFS with explicit staging and raw diagnostic reports.
 * @param provider Same checked provider used by the plan and factor view.
 * @param original Original matrix; its correspondence to factors is
 * caller-owned.
 * @param factor Active calls validate finite positive D and finite E.
 * @param rhs Immutable right-hand sides, not overwritten or silently rescaled.
 * @param solution Initial numeric guess, replaced only after valid native
 * return.
 * @param forward_error Receives the documented estimated forward-error bound.
 * @param backward_error Receives the safeguarded componentwise error estimate.
 * @param plan Matching metadata identity and fixed explicit storage
 * requirements.
 * @param workspace Disjoint live typed buffers as described in this file.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual native INFO/entry, output validity and indexed factor failure.
 * @return Success for finite returned values; kNumerical for invalid factors
 * or published nonfinite output; kProvider for nonzero/unwritten/partial INFO
 * or negative/unwritten estimates. Preflight/provider defects preserve outputs.
 * Estimated diagnostics retain their native qualifications; ordinary INFO0
 * and behavioral fidelity do not certify extreme-value mathematical acceptance.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans DPTRFS using metadata only, without a foreign call.
 * @param provider Explicit accessible host provider matching the factor.
 * @param original Original matrix with its documented lower off-diagonal E.
 * @param factor Nominal immutable real-D/unit-bidiagonal-E PT factors.
 * @param rhs Original N-by-NRHS right-hand sides in either padded full layout.
 * @param solution Input estimate and output refinement, independently laid out.
 * @param forward_error Disjoint contiguous NRHS-length real output vector.
 * @param backward_error Disjoint contiguous NRHS-length real output vector.
 * @return Fixed matching plan or shape/access/alias/native-count/byte failure.
 * Buffer addresses and numeric values may change on immutable-plan reuse;
 * dimensions, physical orientations, strides, layouts and provider must match.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes DPTRFS with explicit staging and raw diagnostic reports.
 * @param provider Same checked provider used by the plan and factor view.
 * @param original Original matrix; its correspondence to factors is
 * caller-owned.
 * @param factor Active calls validate finite positive D and finite E.
 * @param rhs Immutable right-hand sides, not overwritten or silently rescaled.
 * @param solution Initial numeric guess, replaced only after valid native
 * return.
 * @param forward_error Receives the documented estimated forward-error bound.
 * @param backward_error Receives the safeguarded componentwise error estimate.
 * @param plan Matching metadata identity and fixed explicit storage
 * requirements.
 * @param workspace Disjoint live typed buffers as described in this file.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual native INFO/entry, output validity and indexed factor failure.
 * @return Success for finite returned values; kNumerical for invalid factors
 * or published nonfinite output; kProvider for nonzero/unwritten/partial INFO
 * or negative/unwritten estimates. Preflight/provider defects preserve outputs.
 * Estimated diagnostics retain their native qualifications; ordinary INFO0
 * and behavioral fidelity do not certify extreme-value mathematical acceptance.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans CPTRFS using metadata only, without a foreign call.
 * @param provider Explicit accessible host provider matching the factor.
 * @param original Original matrix with its documented lower off-diagonal E.
 * @param factor Nominal immutable real-D/unit-bidiagonal-E PT factors.
 * @param rhs Original N-by-NRHS right-hand sides in either padded full layout.
 * @param solution Input estimate and output refinement, independently laid out.
 * @param forward_error Disjoint contiguous NRHS-length real output vector.
 * @param backward_error Disjoint contiguous NRHS-length real output vector.
 * @return Fixed matching plan or shape/access/alias/native-count/byte failure.
 * Buffer addresses and numeric values may change on immutable-plan reuse;
 * dimensions, physical orientations, strides, layouts and provider must match.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes CPTRFS with explicit staging and raw diagnostic reports.
 * @param provider Same checked provider used by the plan and factor view.
 * @param original Original matrix; its correspondence to factors is
 * caller-owned.
 * @param factor Active calls validate finite positive D and finite E.
 * @param rhs Immutable right-hand sides, not overwritten or silently rescaled.
 * @param solution Initial numeric guess, replaced only after valid native
 * return.
 * @param forward_error Receives the documented estimated forward-error bound.
 * @param backward_error Receives the safeguarded componentwise error estimate.
 * @param plan Matching metadata identity and fixed explicit storage
 * requirements.
 * @param workspace Disjoint live typed buffers as described in this file.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual native INFO/entry, output validity and indexed factor failure.
 * @return Success for finite returned values; kNumerical for invalid factors
 * or published nonfinite output; kProvider for nonzero/unwritten/partial INFO
 * or negative/unwritten estimates. Preflight/provider defects preserve outputs.
 * Estimated diagnostics retain their native qualifications; ordinary INFO0
 * and behavioral fidelity do not certify extreme-value mathematical acceptance.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans ZPTRFS using metadata only, without a foreign call.
 * @param provider Explicit accessible host provider matching the factor.
 * @param original Original matrix with its documented lower off-diagonal E.
 * @param factor Nominal immutable real-D/unit-bidiagonal-E PT factors.
 * @param rhs Original N-by-NRHS right-hand sides in either padded full layout.
 * @param solution Input estimate and output refinement, independently laid out.
 * @param forward_error Disjoint contiguous NRHS-length real output vector.
 * @param backward_error Disjoint contiguous NRHS-length real output vector.
 * @return Fixed matching plan or shape/access/alias/native-count/byte failure.
 * Buffer addresses and numeric values may change on immutable-plan reuse;
 * dimensions, physical orientations, strides, layouts and provider must match.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes ZPTRFS with explicit staging and raw diagnostic reports.
 * @param provider Same checked provider used by the plan and factor view.
 * @param original Original matrix; its correspondence to factors is
 * caller-owned.
 * @param factor Active calls validate finite positive D and finite E.
 * @param rhs Immutable right-hand sides, not overwritten or silently rescaled.
 * @param solution Initial numeric guess, replaced only after valid native
 * return.
 * @param forward_error Receives the documented estimated forward-error bound.
 * @param backward_error Receives the safeguarded componentwise error estimate.
 * @param plan Matching metadata identity and fixed explicit storage
 * requirements.
 * @param workspace Disjoint live typed buffers as described in this file.
 * @param report Metadata aliases preserve it; otherwise reset before preflight.
 * Carries actual native INFO/entry, output validity and indexed factor failure.
 * @return Success for finite returned values; kNumerical for invalid factors
 * or published nonfinite output; kProvider for nonzero/unwritten/partial INFO
 * or negative/unwritten estimates. Preflight/provider defects preserve outputs.
 * Estimated diagnostics retain their native qualifications; ordinary INFO0
 * and behavioral fidelity do not certify extreme-value mathematical acceptance.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_REFINEMENT_H_
