#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_CONDITION_H_
/** @file
 * @brief Explicit condition estimation from immutable separate RK factors.
 *
 * SYCON_3 estimates the reciprocal one-norm condition number of real/complex
 * symmetric matrices; HECON_3 handles complex Hermitian matrices. Complex
 * norms use Euclidean scalar modulus. RCOND is an estimate, not an accuracy
 * certificate. ASC requires a finite nonnegative original matrix norm, even
 * for n=0; this is explicit ASC validation beyond the native negative test.
 *
 * Immutable square A, contiguous same-scalar exact-n E, and exact-n kRook
 * pivots must share provider/scalar/triangle and complete TF2_RK/TRF_RK origin.
 * Numerical content and common provenance are caller preconditions; completed
 * singular raw factors are accepted. A contains D's diagonal and strict unit
 * U/L coefficients with global permutations. Upper E(i)=D(i-1,i), lower
 * E(i)=D(i+1,i), one-based. Boundary, 1-block and unused-partner E slots are
 * not read or numerically validated. Full raw complex factor coefficients
 * are retained. Existing interleaved ROOK factor factories are unchanged and
 * do not certify these inputs. Each signed pivot targets at most its own
 * index for upper and at least its own index for lower; negative pairs encode
 * two independent interchanges. No singular-divisor/finiteness scan is added.
 *
 * Formula queries inspect the supplied norm and descriptor metadata only;
 * output RCOND contents are not read. Plans bind routine/scalar/provider ABI,
 * triangle/symmetry, shapes, E/pivot metadata and original/effective A strides
 * and layout, plus the active zero-norm branch. Positive norms may reuse a
 * plan. Original LDA must fit native INTEGER even if unused. Active n>0 and
 * norm>0 require 2*n live T scalar WORK entries, n private provider INTEGER
 * pivots, and n additional native IWORK entries for real T only. IPIV [0,n)
 * and real IWORK [n,2*n) share kInteger; execution begins their trivial
 * lifetimes in caller byte storage. Row-major A adds n*n live T layout entries;
 * column A and E are direct inputs. No LWORK, foreign workspace query or
 * returned WORK scalar exists. LACN2's 3*n expression, packing products and
 * byte totals are checked independently.
 *
 * After metadata/alias/plan checks, n=0 writes RCOND=1 and nonempty norm=0
 * writes RCOND=0. These successful noncalls require no scratch and inspect no
 * numerical arrays or pivot values. With positive norm the actual native
 * zero 1-block early return remains INFO=0/RCOND=0, without a positive INFO
 * or invented singular pivot diagnosis. Full-width INFO begins at INTEGER
 * minimum and RCOND at -1 immediately before the native call. Nonzero or
 * missing/partial INFO, changed private input pivots, or negative RCOND are
 * provider defects with raw INFO and unusable output. RCOND may already have
 * changed. Nonfinite RCOND is retained with kNumerical, kAccuracyWarning and
 * kDocumentedPartial; finite nonnegative RCOND completes without clamping or
 * certifying its accuracy. Zero may reflect estimator overflow/underflow.
 *
 * A/E/pivots/RCOND, scratch and live provider/plan/workspace/report metadata
 * must be disjoint and accessible to the explicit serial CPU context.
 * Structural failures preserve RCOND, numerical inputs and scratch; metadata
 * aliases also preserve report, otherwise report resets before preflight.
 * Native A/E/pivots stay immutable. Calls allocate nothing, transfer nothing
 * and change no global state. Concurrent calls may share immutable inputs,
 * provider and plans with private writable RCOND/workspace/report storage.
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
/** @brief Queries single real Sycon3 workspace without array reads.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Output object used only for alias validation.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors,
    DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition);
/** @brief Executes single real Sycon3 with immutable RK inputs.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Native estimate or documented empty/zero-norm
 * value.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural failure, provider defect or nonfinite-estimate
 * warning.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sycon3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const float> factors,
       DenseBlasVectorView<const float> off_diagonal, RawLapackPivotView pivots,
       float original_norm, float& reciprocal_condition,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);
/** @brief Queries double real Sycon3 workspace without array reads.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Output object used only for alias validation.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> off_diagonal, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition);
/** @brief Executes double real Sycon3 with immutable RK inputs.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Native estimate or documented empty/zero-norm
 * value.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural failure, provider defect or nonfinite-estimate
 * warning.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sycon3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const double> factors,
       DenseBlasVectorView<const double> off_diagonal,
       RawLapackPivotView pivots, double original_norm,
       double& reciprocal_condition, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex symmetric Sycon3 workspace without array
 * reads.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Output object used only for alias validation.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition);
/** @brief Executes single complex symmetric Sycon3 with immutable RK inputs.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Native estimate or documented empty/zero-norm
 * value.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural failure, provider defect or nonfinite-estimate
 * warning.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sycon3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<float>> factors,
       DenseBlasVectorView<const std::complex<float>> off_diagonal,
       RawLapackPivotView pivots, float original_norm,
       float& reciprocal_condition, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex symmetric Sycon3 workspace without array
 * reads.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Output object used only for alias validation.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySycon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition);
/** @brief Executes double complex symmetric Sycon3 with immutable RK inputs.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Native estimate or documented empty/zero-norm
 * value.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural failure, provider defect or nonfinite-estimate
 * warning.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sycon3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<double>> factors,
       DenseBlasVectorView<const std::complex<double>> off_diagonal,
       RawLapackPivotView pivots, double original_norm,
       double& reciprocal_condition, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex Hermitian Hecon3 workspace without array
 * reads.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Output object used only for alias validation.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHecon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> off_diagonal,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition);
/** @brief Executes single complex Hermitian Hecon3 with immutable RK inputs.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Native estimate or documented empty/zero-norm
 * value.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural failure, provider defect or nonfinite-estimate
 * warning.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hecon3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<float>> factors,
       DenseBlasVectorView<const std::complex<float>> off_diagonal,
       RawLapackPivotView pivots, float original_norm,
       float& reciprocal_condition, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double complex Hermitian Hecon3 workspace without array
 * reads.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Output object used only for alias validation.
 * @return Formula plan or structural, placement, alias or arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHecon3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> off_diagonal,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition);
/** @brief Executes double complex Hermitian Hecon3 with immutable RK inputs.
 * @param provider Explicit same-build serial CPU provider; unchanged.
 * @param triangle Selected upper/lower RK factor triangle.
 * @param factors Immutable square raw RK factors with documented origin.
 * @param off_diagonal Immutable contiguous exact-n same-operation E storage.
 * @param pivots Immutable exact-n same-operation kRook signed paired pivots.
 * @param original_norm Finite nonnegative one-norm of the original matrix.
 * @param reciprocal_condition Native estimate or documented empty/zero-norm
 * value.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Disjoint live scalar, layout and provider INTEGER storage.
 * @param report Mandatory raw INFO, provenance and output-validity report.
 * @return OK, structural failure, provider defect or nonfinite-estimate
 * warning.
 * @pre A/E/pivots share the documented provider and factor operation origin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hecon3(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<const std::complex<double>> factors,
       DenseBlasVectorView<const std::complex<double>> off_diagonal,
       RawLapackPivotView pivots, double original_norm,
       double& reciprocal_condition, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_CONDITION_H_
