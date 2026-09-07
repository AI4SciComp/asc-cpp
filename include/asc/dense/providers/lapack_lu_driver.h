#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_DRIVER_H_

/** @file
 * @brief Explicit Reference-LAPACK general-system expert driver modes.
 *
 * Gesvx selects FACT=N, preserving A/B and producing separate LU/pivots.
 * GesvxEquilibrated selects FACT=E: the provider may scale A/B in place,
 * computes R/C, and reports which scaling it actually applied. GesvxFactored
 * selects FACT=F: A must already have the supplied equilibration and AF/pivots
 * must factor that A. It preserves A/AF/pivots/R/C, but may scale original B.
 * No route fabricates raw-factor provenance or implicitly preserves originals.
 * X always solves the original, unequilibrated system on completed solves.
 *
 * All matrix layouts are independent. Caller kLayoutConversion storage packs
 * each row-major A/AF/B/X consecutively, without transposing mathematical
 * entries. Output-only AF/X packing is not initialized from those outputs.
 * Actual foreign leading dimensions are ABI-sized; original source strides
 * remain ASC-sized plan metadata. All descriptors are host/pinned-host,
 * vectors are contiguous and exact length, and live operands/statistics/
 * equilibration output/workspace spans are pairwise disjoint.
 *
 * Formula-only queries bind mode, transpose, all shapes, layouts, source and
 * foreign leading dimensions, vector counts/strides, and provider identity.
 * They never call the provider or mutate numerical storage. Real WORK has
 * max(1,4*n) entries; complex WORK has 2*n entries and RWORK max(1,2*n).
 * kInteger contains n ABI-width pivots, followed by n disjoint IWORK entries
 * for real routines. Scalar/real/packing objects must already be live; the
 * implementation starts trivial foreign integer lifetimes in caller storage.
 * There is no allocation, hidden packing, transfer, fallback or handler change.
 *
 * Supplied FACT=F scales are finite positive where selected by equilibration;
 * unused scale vectors may have length zero or n and are never read. A/AF/
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
 * FACT=N/E reports identify LU partial pivots; only a complete success report
 * can form a successful factor view. FACT=F does not certify supplied raw LU.
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
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Actual GESVX row/column scaling, not a request to force scaling. */
enum class LapackEquilibration : std::uint8_t {
  kNone,     ///< A is unscaled; upstream EQUED=N.
  kRows,     ///< A is diag(R)*original_A; upstream EQUED=R.
  kColumns,  ///< A is original_A*diag(C); upstream EQUED=C.
  kBoth      ///< A is diag(R)*original_A*diag(C); upstream EQUED=B.
};

/** @brief Caller-owned GESVX diagnostics with source-defined partial validity.
 * @tparam Real float or double, matching the matrix's underlying real scalar.
 */
template <typename Real>
  requires(std::same_as<Real, float> || std::same_as<Real, double>)
struct LapackSolveStatistics {
  Real reciprocal_condition = 0;  ///< RCOND of A after any equilibration.
  Real reciprocal_pivot_growth =
      0;  ///< Raw WORK(1)/RWORK(1) growth diagnostic.
};

/** @brief Queries checked workspace for single real GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single real GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
Gesvx(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      DenseBlasMatrixView<const float> original,
      DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
      DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error,
      LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single real GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original n-by-n A, potentially equilibrated in
 * place.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
QueryGesvxEquilibratedWorkspace(const ReferenceLapackProvider& provider,
                                DenseBlasTranspose transpose,
                                DenseBlasMatrixView<float> original,
                                DenseBlasMatrixView<float> factors,
                                DenseBlasVectorView<index_t> pivots,
                                const LapackEquilibration& equilibration,
                                DenseBlasVectorView<float> row_scales,
                                DenseBlasVectorView<float> column_scales,
                                DenseBlasMatrixView<float> rhs,
                                DenseBlasMatrixView<float> solution,
                                DenseBlasVectorView<float> forward_error,
                                DenseBlasVectorView<float> backward_error,
                                const LapackSolveStatistics<float>& statistics);

/** @brief Executes single real GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original n-by-n A, potentially equilibrated in place.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single real GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Immutable corresponding raw n-by-n LU factors.
 * @param pivots Unchanged. Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single real GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Immutable corresponding raw n-by-n LU factors.
 * @param pivots Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double real GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double real GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
ASC_DENSE_LAPACK_EXPORT Status Gesvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double real GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original n-by-n A, potentially equilibrated in
 * place.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double real GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original n-by-n A, potentially equilibrated in place.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double real GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Immutable corresponding raw n-by-n LU factors.
 * @param pivots Unchanged. Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double real GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Immutable corresponding raw n-by-n LU factors.
 * @param pivots Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single complex GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single complex GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
Gesvx(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      DenseBlasMatrixView<const std::complex<float>> original,
      DenseBlasMatrixView<std::complex<float>> factors,
      DenseBlasVectorView<index_t> pivots,
      DenseBlasMatrixView<const std::complex<float>> rhs,
      DenseBlasMatrixView<std::complex<float>> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error,
      LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single complex GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original n-by-n A, potentially equilibrated in
 * place.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single complex GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original n-by-n A, potentially equilibrated in place.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for single complex GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Immutable corresponding raw n-by-n LU factors.
 * @param pivots Unchanged. Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const LapackSolveStatistics<float>& statistics);

/** @brief Executes single complex GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Immutable corresponding raw n-by-n LU factors.
 * @param pivots Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    LapackSolveStatistics<float>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double complex GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double complex GESVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
ASC_DENSE_LAPACK_EXPORT Status Gesvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double complex GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Original n-by-n A, potentially equilibrated in
 * place.
 * @param factors Unchanged. Separate n-by-n LU output; initial entries are not
 * read.
 * @param pivots Unchanged. Contiguous n-entry one-based LU pivot output,
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
QueryGesvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    const LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double complex GESVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Original n-by-n A, potentially equilibrated in place.
 * @param factors Separate n-by-n LU output; initial entries are not read.
 * @param pivots Contiguous n-entry one-based LU pivot output, initially unread.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots, LapackEquilibration& equilibration,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries checked workspace for double complex GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Unchanged. Original A, transpose(A), or
 * conjugate-transpose(A).
 * @param original Unchanged. Immutable n-by-n A; already equilibrated for
 * supplied-factor mode.
 * @param factors Unchanged. Immutable corresponding raw n-by-n LU factors.
 * @param pivots Unchanged. Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const LapackSolveStatistics<double>& statistics);

/** @brief Executes double complex GESVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param transpose Original A, transpose(A), or conjugate-transpose(A).
 * @param original Immutable n-by-n A; already equilibrated for supplied-factor
 * mode.
 * @param factors Immutable corresponding raw n-by-n LU factors.
 * @param pivots Immutable corresponding n-entry one-based LU pivots.
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
ASC_DENSE_LAPACK_EXPORT Status GesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, LapackEquilibration equilibration,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    LapackSolveStatistics<double>& statistics, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_DRIVER_H_
