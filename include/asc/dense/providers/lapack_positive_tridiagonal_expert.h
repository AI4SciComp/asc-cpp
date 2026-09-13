#ifndef ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_EXPERT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_EXPERT_H_

/** @file
 * @brief Explicit Reference PTSVX with typed fresh and supplied PT factors.
 *
 * Original real D and scalar lower E describe a symmetric/Hermitian positive
 * definite tridiagonal matrix. A mutable factor descriptor selects FACT=N;
 * a nominal PT factor view selects FACT=F. Neither an implicit mode nor a
 * fallback is used. Native PTSVX always uses lower factors; supplied complex
 * upper factors are explicitly conjugated into lower input scratch.
 *
 * Original A/B and supplied factors remain immutable. Generated D/E factors
 * are direct native outputs. X and the caller's RCOND/FERR/BERR are outputs,
 * never pre-read. X and estimates publish only after validated native return;
 * invalid INFO or missing/negative estimates preserve these caller outputs.
 * Generated factor writes cannot be rolled back. FACT=N INFO in 1..N publishes
 * RCOND=0 and partial factors, preserving X/FERR/BERR. Such INFO under FACT=F
 * is invalid provider protocol. INFO=N+1 is a condition
 * warning after the computed solution and estimates. Raw nonfinite results
 * are retained with a numerical warning; no clamp or rescaling is performed.
 *
 * Workspace is explicit and allocation-free: native real 2N or complex N
 * plus real N, staged column X, row B packing, optional supplied-upper E
 * conjugation, and two flat NRHS real estimate arrays. All nine operands,
 * workspace and metadata must be disjoint. Queries and rejected workspace
 * plans inspect metadata only. Plans bind shape/layout/mode/orientation and
 * provider, allowing changed addresses/values and independent concurrent use
 * with separate caller buffers, workspaces, reports and contexts.
 *
 * N=0 completes locally with RCOND=1 and zero estimates, without native INFO
 * or numeric input reads. N>0, NRHS=0 still factors/estimates condition through
 * the provider. Its zero-length solution and estimates are inactive. Generated
 * lower factors may be borrowed through the existing FromRaw factory or
 * explicitly copied for PTTRS reuse; no PTTRF report is manufactured.
 * The provider and its estimated diagnostics retain their documented domain
 * and numerical limitations. Link ASC::dense_lapack; cost is O(N+N*NRHS).
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
/** @brief Plans SPTSVX with generated FACT=N, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    LapackPositiveDefiniteTridiagonalView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes SPTSVX with generated FACT=N and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    LapackPositiveDefiniteTridiagonalView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans SPTSVX with supplied FACT=F, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes SPTSVX with supplied FACT=F and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const float> original,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans DPTSVX with generated FACT=N, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    LapackPositiveDefiniteTridiagonalView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes DPTSVX with generated FACT=N and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    LapackPositiveDefiniteTridiagonalView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans DPTSVX with supplied FACT=F, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes DPTSVX with supplied FACT=F and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const double> original,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans CPTSVX with generated FACT=N, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes CPTSVX with generated FACT=N and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans CPTSVX with supplied FACT=F, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes CPTSVX with supplied FACT=F and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<float>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans ZPTSVX with generated FACT=N, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes ZPTSVX with generated FACT=N and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Writable real D/scalar lower E factor outputs; old numbers are
 * not inputs.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans ZPTSVX with supplied FACT=F, without numeric reads.
 * @param provider Explicit accessible host provider.
 * @param original Immutable original diagonal and lower off-diagonal arrays.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable N-by-NRHS right-hand sides in either full layout.
 * @param solution Output N-by-NRHS matrix, independently laid out.
 * @param reciprocal_condition Disjoint live host output object, never read.
 * @param forward_error Contiguous NRHS underlying-real output estimates.
 * @param backward_error Contiguous NRHS underlying-real output estimates.
 * @return Fixed matching plan or metadata/access/alias/count/byte failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes ZPTSVX with supplied FACT=F and explicit staging.
 * @param provider Provider identity bound by the plan.
 * @param original Original matrix; it is not equilibrated or overwritten.
 * @param factor Immutable nominal factors; upper complex E is conjugated in
 * input packing.
 * @param rhs Immutable right-hand sides.
 * @param solution Receives raw X on validated INFO=0 or INFO=N+1 only.
 * @param reciprocal_condition Receives the raw estimate or documented zero
 * on factorization failure; local N=0 writes one. Old contents are not input.
 * @param forward_error Receives staged estimated forward bounds.
 * @param backward_error Receives staged componentwise backward estimates.
 * @param plan Matching metadata-only workspace plan.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and numerical
 * warning.
 * @return Success for finite results, kNumerical for condition/pivot/nonfinite
 * warnings, kProvider for invalid native protocol, or a preflight error.
 * Supplied active factors require finite positive D and finite E after
 * workspace admission. Preflight preserves all numeric outputs.
 */
ASC_DENSE_LAPACK_EXPORT Status Ptsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<const std::complex<double>> original,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_EXPERT_H_
