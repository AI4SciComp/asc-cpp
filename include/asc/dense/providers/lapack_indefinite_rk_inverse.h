#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_INVERSE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_INVERSE_H_
/** @file
 * @brief Explicit in-place inverses from separate symmetric/Hermitian RK
 * factors.
 *
 * SYTRI_3/SYTRI_3X use transpose symmetry, including complex symmetric data;
 * HETRI_3/HETRI_3X use conjugate transpose. Mutable square A holds D's diagonal
 * and strict unit U/L. Immutable same-scalar exact-n E holds upper D(i-1,i)
 * or lower D(i+1,i), using one-based indices. All n E entries must be live and
 * readable: the native source copies even boundary/1-block/unused-pair slots
 * into WORK before its singular scan. Such ignored slots may contain quiet
 * NaNs and are not numerically validated. Complete raw Hermitian diagonal
 * coefficients are retained; no normalization is applied to stored factors.
 *
 * A/E/exact-n kRook pivots must share one same-provider, same-scalar, same-
 * triangle TF2_RK/TRF_RK operation, including completed singular output.
 * This is global-permutation RK storage. Each signed pivot has its own
 * directional target (upper at most its index, lower at least its index),
 * with adjacent negative entries denoting a 2-block. Numerical content and
 * common origin are caller preconditions; classic/interleaved ROOK factors
 * and existing factor-view factories are not interchangeable with these inputs.
 * Inversion invalidates borrowed views of overwritten factor storage.
 *
 * Only selected A is overwritten by the inverse; the other triangle/padding
 * and E/public pivots remain unchanged. Active WORK needs (n+nb+1)*(nb+3)
 * live T entries. Pinned TRI_3 uses nb=1 for all six variants and always calls
 * TRI_3X for n>0. Its minimum is the raw product; preferred entries also cover
 * the returned rounded real WORK(1). S/C use SROUNDUP_LWORK, D/Z use real
 * conversion. Compute WORK(1) is validated, including zero complex imaginary
 * part. TRI_3X instead takes explicit nb>0, including empty calls; nb>n is
 * allowed within checked bounds. It has no LWORK/query/returned-WORK contract.
 * Active calls need n private provider INTEGER pivots in caller byte storage;
 * row-major A adds n*n live T layout entries. Column-major A and E are direct.
 * Source block dimensions, nested loops/BLAS cursors, driver LWORK and all
 * byte products/sums are checked. Original LDA must fit native INTEGER.
 *
 * Queries inspect metadata only and make no foreign call. Plans bind exact
 * variant, block size, scalar/provider ABI, triangle/symmetry, shapes and
 * original/effective layout/stride/vector metadata. N=0 succeeds after
 * metadata/plan validation with no arrays, workspace or provider call. This
 * ASC noncall does not repair the pinned CSY/ZSY/ZHE TRI_3 empty execution
 * defect: those sources return INFO=0 without writing documented WORK(1).
 *
 * All numerical operands, scratch and live metadata must be disjoint and
 * accessible to the explicit CPU context. Structural errors preserve numerical
 * buffers and scratch. Metadata/report alias rejection also preserves report;
 * otherwise it resets before later preflight. Calls allocate nothing, transfer
 * nothing, change no global state and use no fallback algorithm/provider.
 * Concurrent calls may share immutable E/pivots/providers/plans with distinct
 * writable A/workspace/report storage.
 *
 * INFO>0 identifies an exactly zero complete scalar 1-block D entry: upper
 * scan n..1, lower 1..n. Native E copying precedes the scan but A is unchanged.
 * The report retains one-based INFO and a zero-based diagnostic index, with
 * kSingular/kDocumentedPartial; original selected factors remain, no inverse.
 * No 2-block singularity or finiteness check is added. INFO=0 records
 * completion without certifying a finite or accurate inverse. No
 * RCOND/FERR/BERR exist.
 *
 * Full-width INFO starts at INTEGER minimum after preflight. Unwritten,
 * partial-width, negative or source-inconsistent INFO, changed private input
 * pivots or incorrect driver WORK(1) produce a provider defect with raw INFO
 * and unusable output. Packed A is withheld; direct column-major A may already
 * have changed. Native numerical range and exact-Hermitian-diagonal acceptance
 * remain separate from fidelity; no kernel scaling or output normalization
 * is silently introduced.
 */
#include <complex>

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
/** @brief Queries single real Sytri3 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots);
/** @brief Executes native single real Sytri3 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<float> factors,
       DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries double real Sytri3 workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots);
/** @brief Executes native double real Sytri3 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<double> factors,
       DenseBlasVectorView<const double> off_diagonal,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex symmetric Sytri3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native single complex symmetric Sytri3 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> factors,
       DenseBlasVectorView<const std::complex<float>> off_diagonal,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex symmetric Sytri3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native double complex symmetric Sytri3 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> factors,
       DenseBlasVectorView<const std::complex<double>> off_diagonal,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex Hermitian Hetri3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native single complex Hermitian Hetri3 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> factors,
       DenseBlasVectorView<const std::complex<float>> off_diagonal,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex Hermitian Hetri3 workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native double complex Hermitian Hetri3 in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> factors,
       DenseBlasVectorView<const std::complex<double>> off_diagonal,
       RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single real Sytri3x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots);
/** @brief Executes native single real Sytri3x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<float> factors,
        DenseBlasVectorView<const float> off_diagonal,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double real Sytri3x workspace without array reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots);
/** @brief Executes native double real Sytri3x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<double> factors,
        DenseBlasVectorView<const double> off_diagonal,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex symmetric Sytri3x workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native single complex symmetric Sytri3x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
        DenseBlasVectorView<const std::complex<float>> off_diagonal,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex symmetric Sytri3x workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native double complex symmetric Sytri3x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytri3x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
        DenseBlasVectorView<const std::complex<double>> off_diagonal,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex Hermitian Hetri3x workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native single complex Hermitian Hetri3x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri3x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<float>> factors,
        DenseBlasVectorView<const std::complex<float>> off_diagonal,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex Hermitian Hetri3x workspace without array
 * reads.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetri3xWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots);
/** @brief Executes native double complex Hermitian Hetri3x in place.
 * @param provider Explicit same-build CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor and inverse triangle.
 * @param block_size Positive native block size; checked even at n=0.
 * @param factors Mutable square raw RK factors with documented common origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E; all live.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar/layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, preflight failure, singular numerical result or provider defect.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 * INFO=0 does not certify a finite inverse. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetri3x(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        extent_t block_size, DenseBlasMatrixView<std::complex<double>> factors,
        DenseBlasVectorView<const std::complex<double>> off_diagonal,
        RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_INVERSE_H_
