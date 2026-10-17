#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_INVERSE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_INVERSE_H_

/** @file
 * @brief Explicit pinned CPU inverses from ordinary packed Cholesky factors.
 *
 * PPTRI replaces an upper U or lower L with the selected packed triangle of
 * A inverse, where A=U^H*U or A=L*L^H; H is transpose for real scalars.
 * The input is an unchanged Cholesky factor, with real complex diagonals.
 * All supplied components are passed to native TPTRI, including diagonal
 * imaginary components; no silent Hermitian-input diagonal normalization is
 * performed. Raw storage carries no provenance certificate. No positivity,
 * finiteness or symmetry scan, refactorization or densification is added.
 * The inverse equation assumes valid factors; arbitrary nonreal complex
 * diagonals follow native arithmetic without a new mathematical guarantee.
 *
 * The checked packed descriptor supplies N*(N+1)/2 live T objects in its
 * documented row/column order. Nonempty row input needs exactly that many
 * live T objects in caller kLayoutConversion workspace. Column storage is
 * direct. Query reads metadata only, allocates nothing and enters no provider.
 * Plans bind routine, scalar, order, triangle, layout and exact provider/build/
 * INTEGER ABI. Source bounds cover TPTRI and subsequent packed product loops,
 * including final control values. Empty order completes locally with absent
 * INFO, no numeric reads and no packing. Query work is constant, conversion
 * is O(N^2) and inversion is O(N^3), without performance claims.
 *
 * Preflight validates all operand/workspace and provider/plan/workspace/report
 * metadata before numeric mutation or native entry. Metadata alias rejection
 * preserves report; ordinary structural rejection resets it and preserves
 * all numeric/scratch bytes. Caller storage must satisfy the documented live
 * reachable-span disjointness and workspace capacity/alignment requirements.
 *
 * Native TPTRI scans for exact zero diagonals before mutation. Positive INFO j
 * reports kNumerical/kSingular, index j-1, and the source-defined unchanged
 * factor. Row publication is withheld on that return. Negative, unwritten,
 * partially written or greater-than-N INFO reports a provider defect with
 * signed native INFO and unusable output. Row publication is withheld while
 * direct native writes cannot be rolled back. INFO0 reports native completion,
 * without a finiteness or conditioning certificate. Workspace may change
 * after native entry. Preserve originals separately for inverse verification.
 *
 * Calls borrow accessible host/pinned-host storage synchronously. The caller
 * establishes live typed packing objects and keeps all objects alive. Disjoint
 * calls are reentrant; conflicting mutable access needs caller synchronization.
 * No allocation, implicit transfer, rescaling, fallback, synchronization or
 * provider-handler change occurs. Link ASC::dense_lapack explicitly; native
 * factorizations and other required packed operations remain separate.
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

/** @brief Computes the metadata-only PPTRI packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed Cholesky factors; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> a);

/** @brief Computes the source-defined inverse from packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed factors overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * singular/defect publication, raw-factor, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<float> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes the metadata-only PPTRI packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed Cholesky factors; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> a);

/** @brief Computes the source-defined inverse from packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed factors overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * singular/defect publication, raw-factor, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<double> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes the metadata-only PPTRI packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed Cholesky factors; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a);

/** @brief Computes the source-defined inverse from packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed factors overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * singular/defect publication, raw-factor, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<float>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the metadata-only PPTRI packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed Cholesky factors; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> a);

/** @brief Computes the source-defined inverse from packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed factors overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * singular/defect publication, raw-factor, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<double>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_INVERSE_H_
