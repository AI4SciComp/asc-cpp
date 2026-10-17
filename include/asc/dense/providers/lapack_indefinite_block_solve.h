#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_BLOCK_SOLVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_BLOCK_SOLVE_H_
/** @file
 * @brief Explicit classic symmetric/Hermitian solves using converted factors.
 *
 * SYTRS2 solves transpose-symmetric systems, including complex symmetric
 * systems. HETRS2 solves conjugate-transpose Hermitian systems. A is immutable
 * raw square factor storage and pivots are immutable exact-n kBunchKaufman
 * entries from the same provider, scalar and classic SYTRF/SYTF2 or
 * HETRF/HETF2 operation, including the corresponding actual direct driver.
 * Provenance and numerical content are caller preconditions. Completed
 * singular factors are accepted without adding a source-absent diagnosis.
 * Adjacent equal negative pivots encode one ordered 2-by-2 interchange;
 * unequal rook pairs are not interchangeable.
 *
 * UPLO selects the referenced factor triangle. Complete raw Hermitian factor
 * diagonals/block entries are retained. Native SYCONV temporarily modifies A
 * and restores it before return. Both public layouts therefore use private
 * n*n live T layout storage, keeping every public A byte unchanged on all
 * outcomes. B has n rows and nrhs columns and is overwritten by X. Row-major
 * B needs an additional n*nrhs live T layout entries; its padding is unchanged.
 * Active calls also require n live T scalar WORK entries and n provider-width
 * INTEGER input pivots in caller byte storage. There is no native LWORK,
 * workspace query, or returned WORK-value contract.
 *
 * Metadata-only queries read no numerical arrays and make no foreign call.
 * Plans bind routine, scalar/provider ABI, triangle, order, nrhs and both
 * layouts/leading dimensions. Empty n=0 or nrhs=0 succeeds without array
 * access, workspace or a foreign call, before pivot-value inspection. Active
 * source loop endpoints, strided RHS BLAS cursors, packing products and total
 * bytes are checked before access. All operands, workspace and live metadata
 * must be disjoint and accessible to the explicit CPU context.
 *
 * Structural rejection preserves numerical buffers and scratch. Metadata
 * alias rejection also preserves the report; otherwise it resets before later
 * preflight with no INFO and called_provider=false. Active native INFO starts
 * at the full-width INTEGER minimum; only INFO=0 is valid after preflight.
 * Missing, partial-width or nonzero INFO, altered converted input pivots or
 * unrestored selected factor bytes report a provider defect with raw INFO and
 * unusable output. Row-packed B publication is withheld; direct column-major
 * B may already have changed. Public A and pivots remain unchanged.
 *
 * INFO=0 records native completion without certifying finite values or
 * conditioning. There is no positive singular INFO, RCOND, FERR or BERR.
 * No scalar reciprocal or 2-by-2 solve arithmetic is replaced or normalized.
 * Calls allocate nothing, transfer nothing and change no global state.
 * Concurrent calls may share immutable providers, factors, pivots and plans
 * while using disjoint writable B, workspace and report objects.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Queries single real Sytrs2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs);
/** @brief Executes native single real Sytrs2 with immutable public factors.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
       DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double real Sytrs2 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs);
/** @brief Executes native double real Sytrs2 with immutable public factors.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
       DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex symmetric Sytrs2 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native single complex symmetric Sytrs2 with immutable public
 * factors.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<float>> factors,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries double complex symmetric Sytrs2 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native double complex symmetric Sytrs2 with immutable public
 * factors.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<double>> factors,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries single complex Hermitian Hetrs2 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native single complex Hermitian Hetrs2 with immutable public
 * factors.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetrs2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<float>> factors,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries double complex Hermitian Hetrs2 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrs2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native double complex Hermitian Hetrs2 with immutable public
 * factors.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square raw classic factors with documented origin.
 * @param pivots Immutable exact-n same-operation kBunchKaufman pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre Factors/pivots share the documented provider and operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetrs2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<double>> factors,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_BLOCK_SOLVE_H_
