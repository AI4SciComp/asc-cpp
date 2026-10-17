#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_DRIVER_H_

/** @file
 * @brief Explicit pinned CPU ordinary packed positive-definite system drivers.
 *
 * PPSV factors the selected symmetric/Hermitian A as U^H*U or L*L^H, then
 * solves A*X=B; H is transpose for real scalars. Successful A contains the
 * packed factor and B contains X. Complex input diagonal imaginary parts are
 * ignored. No blanket symmetry, positivity, finiteness or factor-provenance
 * scan, implicit rescaling, replacement algorithm or densification occurs.
 * The actual pinned PPSV entry performs the factorization and solve.
 *
 * A/B row and column layouts are independent. Row A uses p=N*(N+1)/2 live T
 * objects and row B uses N*NRHS live T objects in caller kLayoutConversion
 * storage. Nonempty row B is copied before native entry even if factorization
 * later fails. This explicit conversion preserves B on failure; it is not a
 * claim that native PPSV reads B on that path. Direct B follows native reads.
 * Column operands are passed directly; padding remains unchanged.
 *
 * Only N=0 completes locally without numeric reads or native INFO. N>0 with
 * NRHS=0 still factors A and reports its numerical failure; no B values are
 * read or written. In this case native LDB is max(1,N), while original B
 * stride stays bound in the plan. Queries inspect metadata only and allocate
 * nothing. Plans bind exact routine/scalar/provider/build/INTEGER identity,
 * N/NRHS/actual LDB, triangle, both layouts and original B stride. Integer
 * admission covers both source phases, including final loop controls.
 *
 * Positive INFO j reports kNumerical/kNotPositiveDefinite, index j-1 and
 * kDocumentedPartial factor data; B stays unchanged. Upper publication covers
 * columns through j. Lower j=1 changes only its first diagonal; later failures
 * retain previous trailing updates. Negative, missing, partially written or
 * greater-than-N INFO is a provider defect with signed raw INFO and unusable
 * output. Row publication is withheld on defects; direct writes survive.
 * INFO0 reports completion without a finiteness/conditioning certificate.
 * Preserve original A/B separately for reconstruction and residual checks.
 *
 * Metadata, operand, plan, workspace and report regions must be live and
 * disjoint under their reachable-span contracts. Structural, placement,
 * identity and workspace checks precede numeric mutation/native entry.
 * Metadata alias rejection preserves report; ordinary preflight resets it
 * and preserves all numeric/workspace bytes. Workspace may change after
 * entry. No hidden allocation, transfer, synchronization, fallback or provider
 * handler change occurs. Calls borrow accessible host/pinned-host storage
 * synchronously; callers establish all typed packing lifetimes. Disjoint calls
 * are reentrant; callers exclude conflicting mutable access.
 *
 * Query is constant work, conversion O(N^2+N*NRHS), factorization O(N^3) and
 * solving O(N^2*NRHS), without performance claims. Link ASC::dense_lapack
 * explicitly; this is separate from provider-free native factorizations.
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

/** @brief Computes PPSV's metadata-only caller packing plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed mutable ordinary packed matrix of order N.
 * @param b Borrowed mutable RHS with N rows and any nonnegative column count.
 * @return Matching plan or structural/overflow failure; no numeric reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> a, DenseBlasMatrixView<float> b);

/** @brief Factors packed A and solves its positive-definite system.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed selected input overwritten with complete/partial factors.
 * @param b Borrowed RHS overwritten with X only on the documented success path.
 * @param plan Unmodified matching query result, including original B stride.
 * @param workspace Disjoint live caller objects with queried capacities.
 * @param report Mandatory surviving native INFO, outcome and output validity.
 * @return Success or structural/numerical/provider failure under the file-level
 * mutation, zero-RHS factorization, lifetime, memory and concurrency contracts.
 */
ASC_DENSE_LAPACK_EXPORT Status Ppsv(const ReferenceLapackProvider& provider,
                                    DenseBlasTriangle triangle,
                                    DenseBlasPackedMatrixView<float> a,
                                    DenseBlasMatrixView<float> b,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Computes PPSV's metadata-only caller packing plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed mutable ordinary packed matrix of order N.
 * @param b Borrowed mutable RHS with N rows and any nonnegative column count.
 * @return Matching plan or structural/overflow failure; no numeric reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> a, DenseBlasMatrixView<double> b);

/** @brief Factors packed A and solves its positive-definite system.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed selected input overwritten with complete/partial factors.
 * @param b Borrowed RHS overwritten with X only on the documented success path.
 * @param plan Unmodified matching query result, including original B stride.
 * @param workspace Disjoint live caller objects with queried capacities.
 * @param report Mandatory surviving native INFO, outcome and output validity.
 * @return Success or structural/numerical/provider failure under the file-level
 * mutation, zero-RHS factorization, lifetime, memory and concurrency contracts.
 */
ASC_DENSE_LAPACK_EXPORT Status Ppsv(const ReferenceLapackProvider& provider,
                                    DenseBlasTriangle triangle,
                                    DenseBlasPackedMatrixView<double> a,
                                    DenseBlasMatrixView<double> b,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Computes PPSV's metadata-only caller packing plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed mutable ordinary packed matrix of order N.
 * @param b Borrowed mutable RHS with N rows and any nonnegative column count.
 * @return Matching plan or structural/overflow failure; no numeric reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b);

/** @brief Factors packed A and solves its positive-definite system.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed selected input overwritten with complete/partial factors.
 * @param b Borrowed RHS overwritten with X only on the documented success path.
 * @param plan Unmodified matching query result, including original B stride.
 * @param workspace Disjoint live caller objects with queried capacities.
 * @param report Mandatory surviving native INFO, outcome and output validity.
 * @return Success or structural/numerical/provider failure under the file-level
 * mutation, zero-RHS factorization, lifetime, memory and concurrency contracts.
 */
ASC_DENSE_LAPACK_EXPORT Status Ppsv(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes PPSV's metadata-only caller packing plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed mutable ordinary packed matrix of order N.
 * @param b Borrowed mutable RHS with N rows and any nonnegative column count.
 * @return Matching plan or structural/overflow failure; no numeric reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPpsvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b);

/** @brief Factors packed A and solves its positive-definite system.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Selected upper/lower A storage; invalid flags are rejected.
 * @param a Borrowed selected input overwritten with complete/partial factors.
 * @param b Borrowed RHS overwritten with X only on the documented success path.
 * @param plan Unmodified matching query result, including original B stride.
 * @param workspace Disjoint live caller objects with queried capacities.
 * @param report Mandatory surviving native INFO, outcome and output validity.
 * @return Success or structural/numerical/provider failure under the file-level
 * mutation, zero-RHS factorization, lifetime, memory and concurrency contracts.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ppsv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasPackedMatrixView<std::complex<double>> a,
     DenseBlasMatrixView<std::complex<double>> b,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_DRIVER_H_
