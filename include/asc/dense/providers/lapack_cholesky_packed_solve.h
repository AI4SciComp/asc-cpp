#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_SOLVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_SOLVE_H_

/** @file
 * @brief Explicit pinned CPU solves reusing ordinary packed Cholesky factors.
 *
 * PPTRS overwrites B with X solving (U^H*U)X=B or (L*L^H)X=B, using
 * transpose for real scalars. A supplies the selected raw triangular factor
 * in checked ordinary packed storage and stays unchanged. The caller keeps
 * factors alive and unchanged between uses. All factor components, including
 * complex diagonal imaginary components, are read as supplied; PPTRF's ignored
 * Hermitian-input diagonal rule does not apply to these triangular factors.
 * No refactorization, factor provenance certificate or finiteness scan occurs.
 *
 * A/B layouts are independent. Nonempty row A uses N*(N+1)/2 live T packing
 * objects and row B uses N*NRHS live T objects in caller kLayoutConversion
 * storage. Column storage is passed directly; padding stays unchanged.
 * Queries inspect metadata only and allocate nothing. Plans bind routine,
 * scalar, order/RHS count, actual foreign leading dimension, both layouts,
 * original B stride and exact provider/build/integer ABI. Source bounds cover
 * both TPSV cursors and final loop controls. Empty order or RHS returns locally
 * with absent INFO and no packing or numeric reads. Local dimensions remain
 * in the ASC extent domain without narrowing unexecuted foreign arguments.
 *
 * All operand/workspace/report/plan/provider regions must be disjoint as
 * documented by their reachable spans. Validate shapes, flags, placement,
 * identities and workspace capacity/alignment before mutation or native entry.
 * Metadata alias rejection preserves report; ordinary preflight resets report
 * and preserves every numerical/workspace byte. No implicit allocation,
 * densification, transfer, fallback, synchronization or handler change occurs.
 *
 * Native PPTRS has no positive numerical INFO result or zero-diagonal scan.
 * Arbitrary or zero-diagonal factors follow native TPSV arithmetic. INFO=0
 * reports native completion, without certifying finite values or usable X.
 * Every nonzero, unwritten or partial INFO is a provider defect with surviving
 * signed raw INFO and unusable output. Row RHS publication is withheld on a
 * defect; direct native writes cannot be rolled back. Workspace may change
 * after entry. Preserve original B separately for residual verification.
 *
 * Calls borrow accessible host/pinned-host storage synchronously. The caller
 * establishes live typed packing objects and keeps all objects alive. Disjoint
 * calls are reentrant; shared mutable storage needs caller synchronization.
 * Query work is constant, conversion is O(N^2+N*NRHS) and solving is
 * O(N^2*NRHS), without performance claims. Link ASC::dense_lapack explicitly.
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

/** @brief Computes PPTRS's metadata-only layout workspace plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; numerical values are not inspected.
 * @param b Borrowed mutable RHS with N rows and any nonnegative RHS count.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> a, DenseBlasMatrixView<float> b);

/** @brief Solves using unchanged raw packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; no factorization or scan occurs.
 * @param b Borrowed RHS overwritten with X under the file-level contract.
 * @param plan Unmodified plan matching all descriptors and provider identity.
 * @param workspace Disjoint caller storage with queried live typed capacities.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return Success or structural/provider failure with the documented mutation,
 * raw-factor, lifetime, memory and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptrs(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<const float> a,
                                     DenseBlasMatrixView<float> b,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes PPTRS's metadata-only layout workspace plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; numerical values are not inspected.
 * @param b Borrowed mutable RHS with N rows and any nonnegative RHS count.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> a, DenseBlasMatrixView<double> b);

/** @brief Solves using unchanged raw packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; no factorization or scan occurs.
 * @param b Borrowed RHS overwritten with X under the file-level contract.
 * @param plan Unmodified plan matching all descriptors and provider identity.
 * @param workspace Disjoint caller storage with queried live typed capacities.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return Success or structural/provider failure with the documented mutation,
 * raw-factor, lifetime, memory and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptrs(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<const double> a,
                                     DenseBlasMatrixView<double> b,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes PPTRS's metadata-only layout workspace plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; numerical values are not inspected.
 * @param b Borrowed mutable RHS with N rows and any nonnegative RHS count.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b);

/** @brief Solves using unchanged raw packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; no factorization or scan occurs.
 * @param b Borrowed RHS overwritten with X under the file-level contract.
 * @param plan Unmodified plan matching all descriptors and provider identity.
 * @param workspace Disjoint caller storage with queried live typed capacities.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return Success or structural/provider failure with the documented mutation,
 * raw-factor, lifetime, memory and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptrs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes PPTRS's metadata-only layout workspace plan.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; numerical values are not inspected.
 * @param b Borrowed mutable RHS with N rows and any nonnegative RHS count.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b);

/** @brief Solves using unchanged raw packed Cholesky factors.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Upper U or lower L storage; invalid flags are rejected.
 * @param a Borrowed const packed factors; no factorization or scan occurs.
 * @param b Borrowed RHS overwritten with X under the file-level contract.
 * @param plan Unmodified plan matching all descriptors and provider identity.
 * @param workspace Disjoint caller storage with queried live typed capacities.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return Success or structural/provider failure with the documented mutation,
 * raw-factor, lifetime, memory and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> a,
      DenseBlasMatrixView<std::complex<double>> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_SOLVE_H_
