#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_DRIVER_H_

/** @file
 * @brief Explicit rook symmetric/Hermitian indefinite direct drivers.
 *
 * SYSV_ROOK uses A=U*D*U^T or L*D*L^T, including complex symmetric A; HESV_ROOK
 * uses conjugate transpose for Hermitian A. D contains rook signed paired
 * 1-by-1/2-by-2 blocks. Only selected input/output triangles are referenced.
 * There is no symmetry detection, blanket finiteness scan or algorithm
 * fallback.
 *
 * The actual upstream driver factors A with TRF_ROOK and solves using
 * TRS_ROOK at every workspace size. Negative pivot partners encode two
 * independent interchanges. Caller workspace controls factorization blocking.
 * Minimum scalar WORK is one; preferred is the checked pinned 64*n requirement
 * with the routine's actual floating query conversions. Caller capacity is
 * used up to preferred, retaining reduced-work factorization selection.
 *
 * Active operations require n provider-width kInteger entries for output
 * pivots. Execution begins their trivial lifetimes in caller byte storage;
 * scalar and layout storage already contain live T objects. Row-major A
 * needs n*n explicit layout entries. Original Hermitian A always needs those
 * entries, including column-major, to avoid reads of ignored imaginary
 * diagonals inside the pinned factorization. Only selected offdiagonals and
 * real original diagonals are packed. Row-major B additionally needs n*nrhs
 * entries. Publication touches only selected A and logical B, never padding.
 *
 * Formula queries read metadata, not values, and do not call LAPACK. Plans
 * bind source routine/scalar/triangle, all shapes/layouts, original/effective
 * strides and exact provider build/ABI. Unused one-column strides normalize
 * to row count while the original remains in the key. A nonempty order with
 * nrhs=0 still factors A, using a valid dummy B and effective LDB=max(1,n).
 * n=0 is a successful noncall with no workspace or numerical reads/writes.
 *
 * Operations are synchronous serial-CPU calls of the explicit optional
 * provider. No hidden allocation, transfer, synchronization, provider change
 * or global-state change occurs. All referenced storage and nonempty scratch
 * must be provider-accessible. Numerical operands, scratch and live metadata
 * are disjoint and remain live through execution. Concurrent operations need
 * disjoint writable buffers/reports; no successful-factor lifetime is owned.
 *
 * Structural failures preserve numerical storage and do not enter LAPACK:
 * native INFO is absent and called_provider is false. Unsafe metadata/report
 * aliases leave the report untouched; otherwise it resets before validation.
 * Foreign INFO in [1,n] retains completed selected raw factors and validated
 * raw pivots; B is unchanged. Exact zero identifies singularity; pinned
 * NaN-pivot positive INFO is a distinct partial outcome. Neither certifies
 * a successful factor or solution. INFO=0 retains rook factors
 * and X, without an added finite/conditioning guarantee. Unexpected signed
 * INFO, malformed native pivots or inconsistent returned WORK are provider
 * defects with unusable output. Packed outputs/public pivots are withheld
 * on such defects; a direct column-major provider buffer may have changed.
 * Every native INFO/IPIV entry and WORK[0] is seeded after preflight, so
 * missing or partial-width writes are rejected. Reports retain the actual
 * driver name. ReferenceRookFactorView::Create admits a successful same-call
 * report for subsequent TRS_ROOK reuse without fabricating a TRF origin.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real SYSV_ROOK without array reads or a foreign call.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<float> rhs);

/** @brief Executes actual single real SYSV_ROOK factor-and-solve.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with rook block factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
         DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
         DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
         const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYSV_ROOK without array reads or a foreign call.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<double> rhs);

/** @brief Executes actual double real SYSV_ROOK factor-and-solve.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with rook block factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvRook(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex SYSV_ROOK with transpose-based symmetry.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Executes actual single complex symmetric SYSV_ROOK.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with rook transpose-based factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
         DenseBlasMatrixView<std::complex<float>> matrix,
         DenseBlasVectorView<index_t> pivots,
         DenseBlasMatrixView<std::complex<float>> rhs,
         const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
         LapackReport& report);

/** @brief Queries double complex SYSV_ROOK with transpose-based symmetry.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Executes actual double complex symmetric SYSV_ROOK.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with rook transpose-based factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
         DenseBlasMatrixView<std::complex<double>> matrix,
         DenseBlasVectorView<index_t> pivots,
         DenseBlasMatrixView<std::complex<double>> rhs,
         const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
         LapackReport& report);

/** @brief Queries single complex HESV_ROOK with Hermitian selected-component
 * input.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Executes actual single complex Hermitian HESV_ROOK.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square A; original imaginary diagonals ignored, factors output.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/selected-packing caller
 * storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
HesvRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
         DenseBlasMatrixView<std::complex<float>> matrix,
         DenseBlasVectorView<index_t> pivots,
         DenseBlasMatrixView<std::complex<float>> rhs,
         const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
         LapackReport& report);

/** @brief Queries double complex HESV_ROOK with Hermitian selected-component
 * input.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Executes actual double complex Hermitian HESV_ROOK.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square A; original imaginary diagonals ignored, factors output.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/selected-packing caller
 * storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
HesvRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
         DenseBlasMatrixView<std::complex<double>> matrix,
         DenseBlasVectorView<index_t> pivots,
         DenseBlasMatrixView<std::complex<double>> rhs,
         const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
         LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_DRIVER_H_
