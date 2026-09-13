#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_DRIVER_H_
/** @file
 * @brief Checked SYSV_RK/HESV_RK factorization and solution with separate E.
 *
 * SYSV_RK uses transpose symmetry, including complex symmetric matrices.
 * HESV_RK uses Hermitian symmetry and ignores the input diagonal's imaginary
 * components. A is square; E and pivots are contiguous exact-n output vectors;
 * B has n rows. All storage and live metadata must be disjoint and accessible
 * to the explicit CPU provider. Queries inspect metadata only, allocate nothing
 * and make no foreign call. Structural rejection preserves numerical buffers
 * and scratch; metadata/report alias rejection also preserves the report.
 * Otherwise the report is reset before subsequent preflight.
 *
 * For n>0, native TRF_RK factorization always runs, including when B has zero
 * columns. The driver calls TRS_3 only after successful factorization. Selected
 * A holds D's diagonal and strict unit U/L, E holds upper D(i-1,i) or lower
 * D(i+1,i), and signed one-based pivots encode global permutations and adjacent
 * 2-blocks with each pivot's own directional target. This is separate RK
 * storage, not classic or interleaved ROOK storage. These outputs originate
 * together in the driver's TRF_RK stage and can be reused together by the
 * checked RK solve, condition and inverse operations under their contracts.
 * The other triangle, padding and unused B storage remain unchanged.
 *
 * Active scalar workspace has minimum one and preferred checked rounded 64*n
 * entries for the pinned provider. Execution uses the supplied capacity up to
 * that preference. Native query-to-INTEGER conversion, returned WORK(1), source
 * cursor counts and byte arithmetic are checked. Caller byte storage holds n
 * private live provider INTEGER pivots. Row A/B and all original Hermitian A
 * use explicit caller-owned packing; E and column B are direct. Original LDA
 * and LDB must fit provider INTEGER even when not used. Zero-column B passes
 * a local dummy with a valid native LDB. N=0 is a metadata-only noncall with
 * no numerical reads, output writes or workspace requirement.
 *
 * Successful factorization overwrites B with X. Source-consistent positive
 * INFO publishes completed singular factors and leaves original B; report
 * records kSingular/kDocumentedPartial and zero-based diagnostic index INFO-1.
 * Full-width INFO, private output pivots, structural E/2-block zero entries
 * and returned scalar WORK are checked before publishing packed outputs or
 * public pivots. Invalid native diagnostics are provider defects; direct A/E/B
 * may already have changed, while packed A/B and public pivots are withheld.
 * INFO=0 records completion without guaranteeing finite or accurate results.
 * The required scalar reciprocal and complex range gates remain separate
 * from provider fidelity. No scaling, fallback, provider change, allocation,
 * transfer or process-global state change is introduced. Concurrent calls
 * share only immutable providers/plans and have private writable storage.
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
/** @brief Queries float SysvRk workspace without array access.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs);
/** @brief Executes native float SysvRk with caller-owned workspace.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, packing and provider INTEGER storage.
 * @param report Mandatory provenance, raw INFO and output-validity report.
 * @return OK, preflight rejection, singular result or native provider defect.
 * @pre Writable buffers are distinct from all live metadata and scratch.
 * See the file contract for publication on failure and numerical limitations.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvRk(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<float> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Queries double SysvRk workspace without array access.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> off_diagonal,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<double> rhs);
/** @brief Executes native double SysvRk with caller-owned workspace.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, packing and provider INTEGER storage.
 * @param report Mandatory provenance, raw INFO and output-validity report.
 * @return OK, preflight rejection, singular result or native provider defect.
 * @pre Writable buffers are distinct from all live metadata and scratch.
 * See the file contract for publication on failure and numerical limitations.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvRk(const ReferenceLapackProvider& provider,
                                      DenseBlasTriangle triangle,
                                      DenseBlasMatrixView<double> matrix,
                                      DenseBlasVectorView<double> off_diagonal,
                                      DenseBlasVectorView<index_t> pivots,
                                      DenseBlasMatrixView<double> rhs,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);
/** @brief Queries std::complex<float> SysvRk workspace without array access.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native std::complex<float> SysvRk with caller-owned
 * workspace.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, packing and provider INTEGER storage.
 * @param report Mandatory provenance, raw INFO and output-validity report.
 * @return OK, preflight rejection, singular result or native provider defect.
 * @pre Writable buffers are distinct from all live metadata and scratch.
 * See the file contract for publication on failure and numerical limitations.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> matrix,
       DenseBlasVectorView<std::complex<float>> off_diagonal,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries std::complex<double> SysvRk workspace without array access.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native std::complex<double> SysvRk with caller-owned
 * workspace.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, packing and provider INTEGER storage.
 * @param report Mandatory provenance, raw INFO and output-validity report.
 * @return OK, preflight rejection, singular result or native provider defect.
 * @pre Writable buffers are distinct from all live metadata and scratch.
 * See the file contract for publication on failure and numerical limitations.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> matrix,
       DenseBlasVectorView<std::complex<double>> off_diagonal,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries std::complex<float> HesvRk workspace without array access.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native std::complex<float> HesvRk with caller-owned
 * workspace.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, packing and provider INTEGER storage.
 * @param report Mandatory provenance, raw INFO and output-validity report.
 * @return OK, preflight rejection, singular result or native provider defect.
 * @pre Writable buffers are distinct from all live metadata and scratch.
 * See the file contract for publication on failure and numerical limitations.
 */
ASC_DENSE_LAPACK_EXPORT Status
HesvRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> matrix,
       DenseBlasVectorView<std::complex<float>> off_diagonal,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries std::complex<double> HesvRk workspace without array access.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native std::complex<double> HesvRk with caller-owned
 * workspace.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper or lower original matrix and factor triangle.
 * @param matrix Mutable square original A, overwritten by separate RK factors.
 * @param off_diagonal Mutable contiguous exact-n same-scalar output E.
 * @param pivots Mutable contiguous exact-n one-based signed RK output pivots.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on native success.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, packing and provider INTEGER storage.
 * @param report Mandatory provenance, raw INFO and output-validity report.
 * @return OK, preflight rejection, singular result or native provider defect.
 * @pre Writable buffers are distinct from all live metadata and scratch.
 * See the file contract for publication on failure and numerical limitations.
 */
ASC_DENSE_LAPACK_EXPORT Status
HesvRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> matrix,
       DenseBlasVectorView<std::complex<double>> off_diagonal,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_DRIVER_H_
