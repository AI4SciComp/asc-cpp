#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_H_

/** @file
 * @brief Explicit pinned CPU Cholesky factorization in ordinary packed storage.
 *
 * PPTRF overwrites the selected packed triangle with U or L satisfying
 * A=U^H*U or A=L*L^H; H is transpose for real scalars. The existing checked
 * DenseBlasPackedMatrixView supplies N*(N+1)/2 live T objects in its documented
 * row/column packed order. Triangle selection is explicit. Complex diagonal
 * imaginary components are ignored and never read. No symmetry scan,
 * densification, precision conversion or alternate factorization occurs.
 *
 * Row-packed input uses exactly N*(N+1)/2 live T objects in caller-owned
 * kLayoutConversion workspace. Column-packed storage is passed directly.
 * Queries inspect metadata only, allocate nothing and call no provider.
 * Plans bind routine/scalar, order, triangle/layout and exact provider/build/
 * integer ABI. Source-specific integer bounds include selected BLAS cursors
 * and final loop-control values. Empty order returns locally with absent INFO
 * and no workspace. Query work is constant; explicit packing is O(N^2) and
 * factorization uses O(N^3) scalar operations, without performance claims.
 *
 * Validate every operand/workspace region and provider/plan/workspace/report
 * metadata before numeric mutation. Detected metadata alias preserves report;
 * other preflight resets it while preserving all numeric and scratch bytes.
 * Stale plans and unsupported placement fail before any native entry.
 *
 * Positive INFO in 1..N reports kNumerical/kNotPositiveDefinite with a
 * zero-based diagnostic index and kDocumentedPartial output. Upper failure
 * publishes columns through the failing pivot only. Lower first-pivot failure
 * publishes only its diagonal; later lower failures retain the complete
 * source-defined trailing update. Untouched future complex diagonal imaginary
 * components remain unchanged. INFO=0 reports completed native execution,
 * without certifying finite values, positive definiteness or usable factors
 * for inputs on which native arithmetic yields NaNs. No finiteness scan is
 * added. Negative, unwritten, partially written or impossible INFO is a
 * provider defect with surviving signed raw INFO and unusable output; row
 * publication is withheld, while direct native writes cannot be rolled back.
 * Workspace may change after entry. Preserve originals for residual checks.
 *
 * Storage is borrowed, synchronous and accessible host/pinned-host only.
 * Caller establishes live typed packing objects and keeps all objects alive.
 * Disjoint calls are reentrant; callers synchronize shared mutable storage.
 * No ASC allocation, transfer, fallback, synchronization or process-wide
 * error-handler change occurs. Link the optional ASC::dense_lapack facet;
 * native coverage and packed solves/inverses remain separately reported.
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

/** @brief Computes the metadata-only PPTRF packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed coefficients; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> a);

/** @brief Computes the source-defined packed Cholesky factorization.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed coefficients overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * partial publication, ignored-input, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptrf(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<float> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes the metadata-only PPTRF packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed coefficients; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> a);

/** @brief Computes the source-defined packed Cholesky factorization.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed coefficients overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * partial publication, ignored-input, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Pptrf(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<double> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes the metadata-only PPTRF packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed coefficients; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> a);

/** @brief Computes the source-defined packed Cholesky factorization.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed coefficients overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * partial publication, ignored-input, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pptrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<float>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the metadata-only PPTRF packing formula.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed packed coefficients; values are not read by the query.
 * @return Matching plan or structural/overflow failure; no provider entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPptrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> a);

/** @brief Computes the source-defined packed Cholesky factorization.
 * @param provider Explicit pinned CPU provider borrowed during the call.
 * @param triangle Stored upper/lower triangle; invalid flags are rejected.
 * @param a Borrowed coefficients overwritten according to the file contract.
 * @param plan Unmodified plan matching the scalar, descriptors and provider.
 * @param workspace Disjoint live caller storage with the queried capacities.
 * @param report Mandatory surviving native INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure with the documented
 * partial publication, ignored-input, lifetime and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pptrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<double>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_H_
