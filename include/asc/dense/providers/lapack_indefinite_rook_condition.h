#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_CONDITION_H_

/** @file
 * @brief Explicit rook block-factor reciprocal-condition estimation.
 *
 * SYCON_ROOK consumes real/complex symmetric factors using transpose;
 * HECON_ROOK consumes complex Hermitian factors using conjugate transpose.
 * RCOND is an estimate of the reciprocal one-norm condition number, not an
 * accuracy certificate. Complex one-norms use Euclidean scalar modulus. The
 * caller supplies a finite nonnegative norm of the original matrix and matching
 * same-provider rook bounded Bunch–Kaufman factor/pivot storage. Raw
 * descriptors cannot prove historical provenance or certify a successful
 * factorization.
 *
 * Only the selected factor triangle is read, including actual full complex
 * block coefficients. These are factors, not original Hermitian input;
 * ignored original diagonal normalization must not be applied. Signed paired
 * one-based pivots are checked with rook upper/lower directional bounds.
 * Each negative partner encodes its own ordered interchange; partners need
 * not be equal. Classic/Aasen/other tags are rejected. Pivots are never
 * normalized into a permutation or accepted as a success certificate.
 *
 * Formula queries inspect metadata and the scalar norm only. Plans bind
 * routine/scalar/triangle, shapes, layout, original/effective strides, the
 * zero-norm branch and exact provider build/ABI. An unused single-column
 * stride is normalized while the original remains part of the plan key.
 * Active calls require 2*n live T scalar entries; n provider-width integers
 * for converted pivots; and, for real T only, n additional simultaneous
 * provider-width IWORK entries. The two integer subregions occupy [0,n)
 * and [n,2*n) of kInteger. Row-major adds n*n live T layout entries; packing
 * reads only selected factors. The LACN2 expression 3*n is checked for both
 * real and complex paths independently of workspace size and byte counts.
 *
 * Calls are synchronous, explicitly selected serial CPU operations. No
 * allocation, transfer, synchronization, fallback or global-state change
 * occurs. Every referenced operand and nonempty scratch span must be
 * provider-accessible. Numerical buffers, scratch and live metadata are
 * disjoint. Borrowed inputs remain immutable and live throughout the call;
 * concurrent calls need separate writable outputs/workspace/reports.
 *
 * Structural failures preserve numerical storage and do not enter LAPACK.
 * INFO is absent and called_provider is false. Unsafe metadata/report aliases
 * leave the report untouched; otherwise it resets before validation. For
 * n=0, RCOND=1; for nonempty zero norm, RCOND=0. These successful noncalls
 * require no scratch and read neither factor nor pivot entries.
 *
 * With positive norm, an exactly zero 1-by-1 D entry retains the actual
 * provider's INFO=0/RCOND=0 early-return behavior. Otherwise an evaluated-zero
 * TRS divisor is rejected before writes with kNumerical/kSingular, its
 * zero-based block index and absent INFO. This is not a finiteness scan.
 * Immediately before the call, full provider-width INFO is initialized to
 * its minimum and RCOND to -1, detecting missing output writes. Any nonzero
 * foreign INFO is a provider defect, retained verbatim with
 * unusable output. Finite nonnegative RCOND is complete; zero can also mean
 * estimator underflow/extreme ill-conditioning and gives no invented pivot
 * diagnosis. Nonfinite RCOND is a retained accuracy warning; negative RCOND
 * is provider-invalid. Inputs and padding are never modified.
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

/** @brief Queries single real SYCON_ROOK without a foreign call or array reads.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square rook symmetric factor; entries unread.
 * @param pivots Matching raw rook block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host output; old value unread.
 * @return Complete formula plan or structural error, without mutation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition);

/** @brief Estimates single real reciprocal condition from rook factors.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square same-provider symmetric factor.
 * @param pivots Matching raw rook signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
SyconRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
          float original_norm, float& reciprocal_condition,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries double real SYCON_ROOK without a foreign call or array reads.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square rook symmetric factor; entries unread.
 * @param pivots Matching raw rook block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host output; old value unread.
 * @return Complete formula plan or structural error, without mutation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition);

/** @brief Estimates double real reciprocal condition from rook factors.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square same-provider symmetric factor.
 * @param pivots Matching raw rook signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
SyconRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
          double original_norm, double& reciprocal_condition,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries single complex SYCON_ROOK using transpose-based symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square complex symmetric factor; entries unread.
 * @param pivots Matching raw rook block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single complex symmetric reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square transpose-based complex symmetric factor.
 * @param pivots Matching raw rook signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
SyconRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<const std::complex<float>> factors,
          RawLapackPivotView pivots, float original_norm,
          float& reciprocal_condition, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex SYCON_ROOK using transpose-based symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square complex symmetric factor; entries unread.
 * @param pivots Matching raw rook block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition);

/** @brief Estimates double complex symmetric reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square transpose-based complex symmetric factor.
 * @param pivots Matching raw rook signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
SyconRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<const std::complex<double>> factors,
          RawLapackPivotView pivots, double original_norm,
          double& reciprocal_condition, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex HECON_ROOK using Hermitian symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square rook Hermitian factor; entries unread.
 * @param pivots Matching raw rook block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHeconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single complex Hermitian reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square rook Hermitian factor, full coefficients.
 * @param pivots Matching raw rook signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
HeconRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<const std::complex<float>> factors,
          RawLapackPivotView pivots, float original_norm,
          float& reciprocal_condition, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex HECON_ROOK using Hermitian symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square rook Hermitian factor; entries unread.
 * @param pivots Matching raw rook block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHeconRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition);

/** @brief Estimates double complex Hermitian reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square rook Hermitian factor, full coefficients.
 * @param pivots Matching raw rook signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
HeconRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<const std::complex<double>> factors,
          RawLapackPivotView pivots, double original_norm,
          double& reciprocal_condition, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_CONDITION_H_
