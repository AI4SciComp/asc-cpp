#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_SOLVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_SOLVE_H_
/** @file
 * @brief Explicit symmetric/Hermitian solves from separate RK factor storage.
 *
 * SYTRS_3 solves transpose-symmetric systems, including complex symmetric
 * systems; HETRS_3 uses conjugate-transpose Hermitian symmetry. Immutable A
 * holds D's diagonal and the strict unit triangular factor U or L. Immutable
 * same-scalar E holds D's 2-by-2 offdiagonals: upper E(i)=D(i-1,i), lower
 * E(i)=D(i+1,i), using one-based indices. Boundary E, 1-block entries and the
 * unused partner of each pair are not referenced or numerically validated.
 * Complete raw factor coefficients, including complex Hermitian diagonals,
 * are retained. This is global-permutation RK storage, not interleaved ROOK
 * storage. Signed adjacent negative pivots encode independent interchanges;
 * each target is at most its index for upper and at least its index for lower.
 *
 * Raw A, E and exact-n kRook pivots must come from the same provider, scalar,
 * triangle and SYTF2_RK/SYTRF_RK or HETF2_RK/HETRF_RK operation. Their
 * numerical content and common origin are caller preconditions. Completed
 * singular raw factors are accepted without adding a source-absent divisor
 * check. Existing factor-view factories are unchanged and do not certify these
 * raw inputs.
 *
 * B has n rows and nrhs columns and is overwritten by X. Native A/E/IPIV are
 * read-only. Active calls need n provider INTEGER pivots in caller byte
 * storage; execution begins those trivial lifetimes. Row-major A adds n*n live
 * T layout entries; row-major B adds n*nrhs. Column-major arrays and E are
 * passed directly. No scalar WORK, LWORK, native workspace query or returned
 * WORK value exists. Queries inspect metadata only. Plans bind
 * routine/scalar/provider ABI, triangle, shapes, vector metadata and
 * original/effective matrix layouts and strides. Source loop endpoints, BLAS
 * terminal cursors and packing/byte products are checked. Empty n=0 or nrhs=0
 * accesses no numerical arrays, requires no workspace and makes no native call,
 * before pivot-value checks.
 *
 * All numerical operands, scratch and live provider/plan/workspace/report
 * metadata must be disjoint and accessible to the explicit serial CPU context.
 * Structural errors preserve numerical buffers and scratch. Metadata aliases
 * also preserve the report; otherwise it resets before subsequent preflight.
 * Native INFO begins at the full-width INTEGER minimum; only zero is valid
 * after preflight. Missing/partial/nonzero INFO or changed private input pivots
 * produce a provider defect with raw INFO and unusable output. Row-packed B
 * publication is withheld; direct column-major B may already have changed.
 *
 * INFO=0 records native completion without a finiteness or conditioning
 * certificate. No positive singular INFO, RCOND, FERR or BERR is introduced.
 * Scalar reciprocal and 2-by-2 arithmetic are not replaced or normalized.
 * Calls allocate nothing, transfer nothing and change no global state.
 * Concurrent calls may share immutable A/E/pivots/providers/plans and require
 * distinct writable B, scratch and report storage.
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
/** @brief Queries single real Sytrs3 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs);
/** @brief Executes native single real Sytrs3 with immutable RK inputs.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const float> factors,
       DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
       DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double real Sytrs3 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs);
/** @brief Executes native double real Sytrs3 with immutable RK inputs.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Sytrs3(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex symmetric Sytrs3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native single complex symmetric Sytrs3 with immutable RK
 * inputs.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<float>> factors,
       DenseBlasVectorView<const std::complex<float>> off_diagonal,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries double complex symmetric Sytrs3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native double complex symmetric Sytrs3 with immutable RK
 * inputs.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytrs3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<double>> factors,
       DenseBlasVectorView<const std::complex<double>> off_diagonal,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries single complex Hermitian Hetrs3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native single complex Hermitian Hetrs3 with immutable RK
 * inputs.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetrs3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<float>> factors,
       DenseBlasVectorView<const std::complex<float>> off_diagonal,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries double complex Hermitian Hetrs3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrs3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native double complex Hermitian Hetrs3 with immutable RK
 * inputs.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides, overwritten by solutions.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural preflight failure or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite solution; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetrs3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<double>> factors,
       DenseBlasVectorView<const std::complex<double>> off_diagonal,
       RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_SOLVE_H_
