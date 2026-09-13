#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EXPERT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EXPERT_H_

/** @file
 * @brief Exact Reference-LAPACK general-band expert driver modes.
 *
 * Gbsvx selects FACT=N, preserving A/B and producing separate LU/pivots.
 * GbsvxEquilibrated selects FACT=E: the provider may scale A/B in place,
 * computes R/C, and reports which scaling it actually applied. GbsvxFactored
 * selects FACT=F: A must already have the supplied equilibration and AF/pivots
 * must factor that A. It preserves AB/AFB/pivots/R/C, but may scale original B.
 * No route fabricates raw-factor provenance or implicitly preserves originals.
 * X always solves the original, unequilibrated system on completed solves.
 *
 * B and X layouts are independent. Caller kLayoutConversion storage packs
 * each row-major B/X consecutively, without transposing mathematical
 * entries. Output-only X packing is not initialized from its initial values.
 * Actual foreign leading dimensions are ABI-sized; original source strides
 * remain ASC-sized plan metadata. All descriptors are host/pinned-host,
 * vectors are contiguous and exact length, and live operands/statistics/
 * equilibration output/workspace spans are pairwise disjoint.
 *
 * Compact AB is column-major with diagonal KU; expanded AFB is column-major
 * with diagonal KL+KU and full fill-in backing. All nonnegative bandwidths
 * are preserved without densification. B/X layouts are independent. Every
 * detected metadata alias preserves report; otherwise report resets before
 * remaining preflight. Every admitted empty call executes the actual source.
 *
 * Formula-only queries bind mode, bandwidths, transpose, all shapes, layouts,
 * source and foreign leading dimensions, vector counts/strides, and provider
 * identity. They never call the provider or mutate numerical storage. Real WORK
 * has max(1,3*n) entries; complex WORK has 2*n entries and RWORK max(1,n).
 * kInteger contains n ABI-width pivots, followed by n disjoint IWORK entries
 * for real routines. Scalar/real/packing objects must already be live; the
 * implementation starts trivial foreign integer lifetimes in caller storage.
 * There is no allocation, hidden packing, transfer, fallback or handler change.
 *
 * Supplied FACT=F scales are finite positive where selected by equilibration;
 * unused scale vectors may have length zero or n and are never read. AB/AFB/
 * pivots/used scales must have common provenance. Exact-zero U is rejected
 * before writes, with kNumerical/kSingular and absent INFO; no tolerance.
 * Structural failure leaves all outputs/workspaces unchanged. For n=0, the
 * source-defined empty outputs are RCOND=1, growth=1, FERR/BERR=0; nrhs=0 does
 * not suppress factorization or condition estimation of nonempty A.
 *
 * Negative or impossible INFO is a provider defect with unusable output.
 * INFO in [1,n] is an exact-zero-U result after completed factorization:
 * AF/pivots, any applied A/B scaling, RCOND=0 and growth are retained;
 * X/FERR/BERR remain unchanged. R/C computed by FACT=E can be partial if its
 * internal equilibration encountered a zero row/column; only scales selected
 * by the returned equilibration are usable. No successful factor is certified.
 * Reports leave factor_family absent; raw band factors and signed sequential
 * band pivots do not certify a successful factor or reinterpret GETRF tags.
 * INFO=n+1 gives kNumerical/kAccuracyWarning with computed X/FERR/BERR and
 * diagnostics retained; this is not exact singularity. Raw INFO is never
 * invented or replaced by an ASC finding.
 *
 * After INFO=0 or n+1, nonfinite estimates/statistics produce an accuracy
 * warning; negative estimates/statistics are provider-invalid. Raw values and
 * completed X are retained with documented-partial validity. Only individual
 * finite nonnegative diagnostics satisfy their output contract. FERR is an
 * estimate, not a guaranteed bound; finite input may expose intermediate
 * overflow in the pinned provider. RCOND describes the equilibrated matrix.
 * A/B/AF entries and X are not scanned for finiteness; no finite-X guarantee
 * is added for arbitrary nonfinite input or unrepresentable solutions.
 * Growth is copied exactly from real WORK(1) or complex RWORK(1), including
 * the leading INFO-column result after singular factorization.
 */

#include <complex>
#include <concepts>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_driver.h"

namespace asc {

/** @brief Queries checked workspace for single real GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param rhs Unchanged. Immutable n-by-nrhs original B.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single real GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbsvx(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      ReferenceGeneralBandView<const float> original,
      LapackLuBandView<float> factors, DenseBlasVectorView<index_t> pivots,
      DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error,
      LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single real GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original compact n-by-n AB, potentially
 * equilibrated in place.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param equilibration Unchanged. Actual scaling output; initial enum value is
 * not read.
 * @param row_scales Unchanged. Contiguous n-entry real row-scale output.
 * @param column_scales Unchanged. Contiguous n-entry real column-scale output.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryGbsvxEquilibratedWorkspace(const ReferenceLapackProvider& provider,
                                DenseBlasTranspose transpose,
                                ReferenceGeneralBandView<float> original,
                                LapackLuBandView<float> factors,
                                DenseBlasVectorView<index_t> pivots,
                                const LapackEquilibration& equilibration,
                                DenseBlasVectorView<float> row_scales,
                                DenseBlasVectorView<float> column_scales,
                                DenseBlasMatrixView<float> rhs,
                                DenseBlasMatrixView<float> solution,
                                DenseBlasVectorView<float> forward_error,
                                DenseBlasVectorView<float> backward_error,
                                const LapackSolveStatistics<float>& statistics);

/** @brief Executes single real GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original compact n-by-n AB, potentially equilibrated in
 * place.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param equilibration Actual scaling output; initial enum value is not read.
 * @param row_scales Contiguous n-entry real row-scale output.
 * @param column_scales Contiguous n-entry real column-scale output.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<float> original, LapackLuBandView<float> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single real GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Immutable expanded band LU factors with diagonal
 * row KL+KU.
 * @param pivots Unchanged. Matching immutable signed sequential band swaps, all
 * n entries validated.
 * @param equilibration Unchanged. Scaling already applied to A, selecting the
 * used R/C vectors.
 * @param row_scales Unchanged. Selected positive row scales; unused length zero
 * or n allowed.
 * @param column_scales Unchanged. Selected positive column scales; unused
 * length zero or n allowed.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single real GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Immutable expanded band LU factors with diagonal row KL+KU.
 * @param pivots Matching immutable signed sequential band swaps, all n entries
 * validated.
 * @param equilibration Scaling already applied to A, selecting the used R/C
 * vectors.
 * @param row_scales Selected positive row scales; unused length zero or n
 * allowed.
 * @param column_scales Selected positive column scales; unused length zero or n
 * allowed.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const float> original,
    LapackLuBandView<const float> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double real GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param rhs Unchanged. Immutable n-by-nrhs original B.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double real GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double real GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original compact n-by-n AB, potentially
 * equilibrated in place.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param equilibration Unchanged. Actual scaling output; initial enum value is
 * not read.
 * @param row_scales Unchanged. Contiguous n-entry real row-scale output.
 * @param column_scales Unchanged. Contiguous n-entry real column-scale output.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<double> original, LapackLuBandView<double> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double real GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original compact n-by-n AB, potentially equilibrated in
 * place.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param equilibration Actual scaling output; initial enum value is not read.
 * @param row_scales Contiguous n-entry real row-scale output.
 * @param column_scales Contiguous n-entry real column-scale output.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<double> original, LapackLuBandView<double> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double real GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Immutable expanded band LU factors with diagonal
 * row KL+KU.
 * @param pivots Unchanged. Matching immutable signed sequential band swaps, all
 * n entries validated.
 * @param equilibration Unchanged. Scaling already applied to A, selecting the
 * used R/C vectors.
 * @param row_scales Unchanged. Selected positive row scales; unused length zero
 * or n allowed.
 * @param column_scales Unchanged. Selected positive column scales; unused
 * length zero or n allowed.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double real GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Immutable expanded band LU factors with diagonal row KL+KU.
 * @param pivots Matching immutable signed sequential band swaps, all n entries
 * validated.
 * @param equilibration Scaling already applied to A, selecting the used R/C
 * vectors.
 * @param row_scales Selected positive row scales; unused length zero or n
 * allowed.
 * @param column_scales Selected positive column scales; unused length zero or n
 * allowed.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const double> original,
    LapackLuBandView<const double> factors, ReferenceLuBandPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single complex GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param rhs Unchanged. Immutable n-by-nrhs original B.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single complex GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbsvx(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      ReferenceGeneralBandView<const std::complex<float>> original,
      LapackLuBandView<std::complex<float>> factors,
      DenseBlasVectorView<index_t> pivots,
      DenseBlasMatrixView<const std::complex<float>> rhs,
      DenseBlasMatrixView<std::complex<float>> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error,
      LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single complex GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original compact n-by-n AB, potentially
 * equilibrated in place.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param equilibration Unchanged. Actual scaling output; initial enum value is
 * not read.
 * @param row_scales Unchanged. Contiguous n-entry real row-scale output.
 * @param column_scales Unchanged. Contiguous n-entry real column-scale output.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<float>> original,
    LapackLuBandView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single complex GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original compact n-by-n AB, potentially equilibrated in
 * place.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param equilibration Actual scaling output; initial enum value is not read.
 * @param row_scales Contiguous n-entry real row-scale output.
 * @param column_scales Contiguous n-entry real column-scale output.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<float>> original,
    LapackLuBandView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single complex GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Immutable expanded band LU factors with diagonal
 * row KL+KU.
 * @param pivots Unchanged. Matching immutable signed sequential band swaps, all
 * n entries validated.
 * @param equilibration Unchanged. Scaling already applied to A, selecting the
 * used R/C vectors.
 * @param row_scales Unchanged. Selected positive row scales; unused length zero
 * or n allowed.
 * @param column_scales Unchanged. Selected positive column scales; unused
 * length zero or n allowed.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single complex GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Immutable expanded band LU factors with diagonal row KL+KU.
 * @param pivots Matching immutable signed sequential band swaps, all n entries
 * validated.
 * @param equilibration Scaling already applied to A, selecting the used R/C
 * vectors.
 * @param row_scales Selected positive row scales; unused length zero or n
 * allowed.
 * @param column_scales Selected positive column scales; unused length zero or n
 * allowed.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<float>> original,
    LapackLuBandView<const std::complex<float>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double complex GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param rhs Unchanged. Immutable n-by-nrhs original B.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double complex GBSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double complex GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original compact n-by-n AB, potentially
 * equilibrated in place.
 * @param factors Unchanged. Separate expanded band LU output; initial entries
 * are not read.
 * @param pivots Unchanged. Contiguous n-entry signed band pivot output,
 * initially unread.
 * @param equilibration Unchanged. Actual scaling output; initial enum value is
 * not read.
 * @param row_scales Unchanged. Contiguous n-entry real row-scale output.
 * @param column_scales Unchanged. Contiguous n-entry real column-scale output.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryGbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double complex GBSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original compact n-by-n AB, potentially equilibrated in
 * place.
 * @param factors Separate expanded band LU output; initial entries are not
 * read.
 * @param pivots Contiguous n-entry signed band pivot output, initially unread.
 * @param equilibration Actual scaling output; initial enum value is not read.
 * @param row_scales Contiguous n-entry real row-scale output.
 * @param column_scales Contiguous n-entry real column-scale output.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<std::complex<double>> original,
    LapackLuBandView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double complex GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable compact n-by-n AB; already equilibrated
 * for supplied-factor mode.
 * @param factors Unchanged. Immutable expanded band LU factors with diagonal
 * row KL+KU.
 * @param pivots Unchanged. Matching immutable signed sequential band swaps, all
 * n entries validated.
 * @param equilibration Unchanged. Scaling already applied to A, selecting the
 * used R/C vectors.
 * @param row_scales Unchanged. Selected positive row scales; unused length zero
 * or n allowed.
 * @param column_scales Unchanged. Selected positive column scales; unused
 * length zero or n allowed.
 * @param rhs Unchanged. Original n-by-nrhs B, potentially scaled in place.
 * @param solution Unchanged. Separate n-by-nrhs X output; initial entries are
 * not read.
 * @param forward_error Unchanged. Contiguous nrhs-entry real FERR output.
 * @param backward_error Unchanged. Contiguous nrhs-entry real BERR output.
 * @param statistics Unchanged. Live disjoint host diagnostics object; raw
 * values are retained.
 * @return Checked formula-only plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double complex GBSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable compact n-by-n AB; already equilibrated for
 * supplied-factor mode.
 * @param factors Immutable expanded band LU factors with diagonal row KL+KU.
 * @param pivots Matching immutable signed sequential band swaps, all n entries
 * validated.
 * @param equilibration Scaling already applied to A, selecting the used R/C
 * vectors.
 * @param row_scales Selected positive row scales; unused length zero or n
 * allowed.
 * @param column_scales Selected positive column scales; unused length zero or n
 * allowed.
 * @param rhs Original n-by-nrhs B, potentially scaled in place.
 * @param solution Separate n-by-nrhs X output; initial entries are not read.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param statistics Live disjoint host diagnostics object; raw values are
 * retained.
 * @param plan Unmodified metadata-bound formula query result.
 * @param workspace Explicit disjoint numerical, integer and packing storage.
 * @param report Mandatory reset-before-preflight failure-surviving diagnostics.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GbsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceGeneralBandView<const std::complex<double>> original,
    LapackLuBandView<const std::complex<double>> factors,
    ReferenceLuBandPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EXPERT_H_
