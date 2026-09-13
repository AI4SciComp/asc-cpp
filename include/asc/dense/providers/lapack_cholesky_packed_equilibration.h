#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_EQUILIBRATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_EQUILIBRATION_H_

/** @file
 * @brief Explicit pinned CPU diagonal scaling for ordinary packed matrices.
 *
 * PPEQU returns S(i)=1/sqrt(real(A(i,i))), AMAX=max(real(diagonal)) and
 * SCOND=sqrt(min(real(diagonal)))/sqrt(max(real(diagonal))). It neither applies
 * scales nor certifies positive definiteness. All A storage stays unchanged;
 * only real diagonal components are read. Off-diagonals and complex imaginary
 * diagonals are ignored. No blanket positivity/finiteness scan, rescaling,
 * densification, fallback or hidden allocation occurs.
 *
 * Both ordinary packed layouts use direct borrowed storage and zero workspace.
 * Row-upper diagonal offsets equal column-lower and conversely, so this
 * diagonal-only native call exchanges UPLO for row storage. This does not
 * transpose a matrix or change the contracts of other packed operations.
 * Queries inspect metadata only, binding exact routine/scalar/provider/build/
 * INTEGER identity, order, original triangle/layout, effective native triangle
 * and scale-vector length/increment. Source integer admission covers actual
 * packed cursor evaluation and loop exits. Query is constant work; execution
 * and output validation are O(N), without performance claims.
 *
 * Positive INFO j reports kNumerical/kNotPositiveDefinite, index j-1 and
 * kDocumentedPartial output: S contains every raw real diagonal, AMAX is
 * complete and SCOND is unchanged. S is not a valid scale vector. Negative,
 * missing, partially written or greater-than-N INFO is a provider defect with
 * signed raw INFO and unusable direct outputs retained. On INFO0, finite
 * positive S, finite SCOND>=0 and finite AMAX>0 are required; otherwise return
 * kNumerical/kAccuracyWarning with raw documented partial outputs. This late
 * check does not establish an input finiteness guarantee. For N=0, SCOND=1,
 * AMAX=0 and S is untouched locally without provider entry or native INFO.
 *
 * S is contiguous underlying-real storage with increment one. A, S, SCOND,
 * AMAX, all workspace and provider/plan/report metadata must have disjoint
 * reachable spans and live containing objects under their existing contracts.
 * Metadata alias rejection preserves report; ordinary preflight resets it.
 * All structural, placement, identity and workspace checks precede numeric
 * writes or native entry. Structural failures preserve all numeric bytes.
 * Calls borrow accessible host/pinned-host storage synchronously. No implicit
 * allocation, transfer, synchronization or process-global handler occurs.
 * Disjoint calls are reentrant; callers exclude conflicting mutable access.
 * Link ASC::dense_lapack explicitly; native factorization coverage is separate.
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

/** @brief Queries the zero-workspace PPEQU metadata plan.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable ordinary packed matrix in either layout, numerically
 * unread.
 * @param scales Disjoint contiguous N-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND object, unread.
 * @param absolute_maximum Disjoint live host AMAX object, unread.
 * @return Matching formula plan or structural/overflow failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> a, DenseBlasVectorView<float> scales,
    const float& scale_condition, const float& absolute_maximum);

/** @brief Computes packed real-diagonal scales without applying them.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable packed matrix; only real diagonal components are input.
 * @param scales Disjoint contiguous underlying-real S output, partial on
 * failure.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum output.
 * @param plan Unmodified matching query result with every option bound.
 * @param workspace Explicit disjoint workspace, validated even when
 * unnecessary.
 * @param report Mandatory failure-surviving native INFO, outcome and validity.
 * @return Success or structural/numerical/provider failure with the file-level
 * mutation, ignored-input, empty-call, lifetime and memory semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppequ(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const float> a,
      DenseBlasVectorView<float> scales, float& scale_condition,
      float& absolute_maximum, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the zero-workspace PPEQU metadata plan.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable ordinary packed matrix in either layout, numerically
 * unread.
 * @param scales Disjoint contiguous N-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND object, unread.
 * @param absolute_maximum Disjoint live host AMAX object, unread.
 * @return Matching formula plan or structural/overflow failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> a,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum);

/** @brief Computes packed real-diagonal scales without applying them.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable packed matrix; only real diagonal components are input.
 * @param scales Disjoint contiguous underlying-real S output, partial on
 * failure.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum output.
 * @param plan Unmodified matching query result with every option bound.
 * @param workspace Explicit disjoint workspace, validated even when
 * unnecessary.
 * @param report Mandatory failure-surviving native INFO, outcome and validity.
 * @return Success or structural/numerical/provider failure with the file-level
 * mutation, ignored-input, empty-call, lifetime and memory semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppequ(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const double> a,
      DenseBlasVectorView<double> scales, double& scale_condition,
      double& absolute_maximum, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the zero-workspace PPEQU metadata plan.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable ordinary packed matrix in either layout, numerically
 * unread.
 * @param scales Disjoint contiguous N-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND object, unread.
 * @param absolute_maximum Disjoint live host AMAX object, unread.
 * @return Matching formula plan or structural/overflow failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasVectorView<float> scales, const float& scale_condition,
    const float& absolute_maximum);

/** @brief Computes packed real-diagonal scales without applying them.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable packed matrix; only real diagonal components are input.
 * @param scales Disjoint contiguous underlying-real S output, partial on
 * failure.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum output.
 * @param plan Unmodified matching query result with every option bound.
 * @param workspace Explicit disjoint workspace, validated even when
 * unnecessary.
 * @param report Mandatory failure-surviving native INFO, outcome and validity.
 * @return Success or structural/numerical/provider failure with the file-level
 * mutation, ignored-input, empty-call, lifetime and memory semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppequ(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> a,
      DenseBlasVectorView<float> scales, float& scale_condition,
      float& absolute_maximum, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the zero-workspace PPEQU metadata plan.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable ordinary packed matrix in either layout, numerically
 * unread.
 * @param scales Disjoint contiguous N-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND object, unread.
 * @param absolute_maximum Disjoint live host AMAX object, unread.
 * @return Matching formula plan or structural/overflow failure; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpequWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> a,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum);

/** @brief Computes packed real-diagonal scales without applying them.
 * @param provider Explicit pinned CPU provider borrowed for the call.
 * @param triangle Selected original packed triangle; invalid flags are
 * rejected.
 * @param a Immutable packed matrix; only real diagonal components are input.
 * @param scales Disjoint contiguous underlying-real S output, partial on
 * failure.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum output.
 * @param plan Unmodified matching query result with every option bound.
 * @param workspace Explicit disjoint workspace, validated even when
 * unnecessary.
 * @param report Mandatory failure-surviving native INFO, outcome and validity.
 * @return Success or structural/numerical/provider failure with the file-level
 * mutation, ignored-input, empty-call, lifetime and memory semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppequ(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> a,
      DenseBlasVectorView<double> scales, double& scale_condition,
      double& absolute_maximum, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_EQUILIBRATION_H_
