#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_CONDITION_H_

/** @file
 * @brief Explicit packed Bunch--Kaufman reciprocal-condition estimates.
 *
 * SPCON consumes real/complex symmetric packed factors using transpose;
 * HPCON consumes complex Hermitian factors using adjoint. RCOND estimates the
 * reciprocal original-matrix one-norm condition number, not solution accuracy.
 * Complex one-norms use Euclidean scalar modulus. Supply a finite nonnegative
 * original norm and matching completed same-provider SPTRF/HPTRF factors and
 * exact-n classic pivots, including completed singular factors. Raw views do
 * not prove provenance or certify successful factorization. Packing preserves
 * complete factor coefficients without original-input diagonal normalization.
 *
 * Queries inspect metadata and the scalar norm, with no native query or array
 * reads. Plans bind routine/scalar/provider, order, triangle/symmetry, packed
 * layout, exact pivot count and the zero-norm branch. A positive norm may vary
 * within a matching plan. Active workspace contains 2*n live T WORK entries,
 * n native INTEGER converted pivots, and, for real T only, n simultaneous
 * native INTEGER IWORK entries. Integer subregions occupy [0,n) and [n,2*n).
 * Row layout adds n*(n+1)/2 live T packing entries. The full packed n*(n+1)
 * product, TRS terminal cursor and LACN2 3*n expressions are bounded before
 * arithmetic or conversion, independently of byte totals. Empty or zero-norm
 * execution requires no scratch and reads neither AP nor pivot values.
 *
 * Operations are synchronous serial CPU calls with an explicit provider.
 * Numerical operands, scratch and live provider/plan/workspace/report metadata
 * must be accessible and disjoint. Structural failure preserves numerical
 * output and scratch; unsafe metadata aliases preserve report too, otherwise
 * report resets before validation. There is no allocation, implicit transfer,
 * fallback or global-state change. Concurrent calls may share immutable AP,
 * pivots, provider and plans with separate output/workspace/reports.
 *
 * Empty order writes RCOND=1; nonempty zero norm writes RCOND=0. These are
 * successful noncalls with no native INFO. Positive norm validates every
 * signed equal-negative pair and UPLO-dependent pivot direction. Any exactly
 * scalar-zero positive-pivot 1x1 D entry preserves the native INFO=0/RCOND=0
 * return. Otherwise evaluated-zero TRS divisors reject before mutation with
 * numerical/singular, a zero-based block index and absent INFO. This is not a
 * finiteness scan or a claim that arbitrary raw factors are valid.
 *
 * Native INFO is seeded at its full-width minimum and must return zero;
 * private native input pivots must remain unchanged. An INFO or pivot defect
 * keeps the caller's RCOND unchanged and reports unusable output with raw INFO.
 * Otherwise the actual native RCOND is published: finite nonnegative values
 * are complete; zero may indicate estimator underflow or ill-conditioning,
 * without an invented pivot diagnosis. Nonfinite values give a retained
 * numerical accuracy warning; negative values are provider-invalid. AP and
 * caller pivots remain immutable. No factor certificate is issued.
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

/** @brief Queries single real SPCON without a foreign call or array reads.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed classic symmetric factor; entries unread.
 * @param pivots Matching raw classic block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host output; old value unread.
 * @return Complete formula plan or structural error, without mutation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    float original_norm, const float& reciprocal_condition);

/** @brief Estimates single real reciprocal condition from classic factors.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed same-provider symmetric factor.
 * @param pivots Matching raw classic signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Spcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
      float original_norm, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double real SPCON without a foreign call or array reads.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed classic symmetric factor; entries unread.
 * @param pivots Matching raw classic block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host output; old value unread.
 * @return Complete formula plan or structural error, without mutation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    double original_norm, const double& reciprocal_condition);

/** @brief Estimates double real reciprocal condition from classic factors.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed same-provider symmetric factor.
 * @param pivots Matching raw classic signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Spcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const double> factors,
      RawLapackPivotView pivots, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex SPCON using transpose-based symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square complex symmetric factor; entries unread.
 * @param pivots Matching raw classic block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single complex symmetric reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square transpose-based complex symmetric factor.
 * @param pivots Matching raw classic signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Spcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> factors,
      RawLapackPivotView pivots, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex SPCON using transpose-based symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square complex symmetric factor; entries unread.
 * @param pivots Matching raw classic block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition);

/** @brief Estimates double complex symmetric reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable square transpose-based complex symmetric factor.
 * @param pivots Matching raw classic signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Spcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> factors,
      RawLapackPivotView pivots, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex HPCON using Hermitian symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed classic Hermitian factor; entries unread.
 * @param pivots Matching raw classic block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, float original_norm,
    const float& reciprocal_condition);

/** @brief Estimates single complex Hermitian reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed classic Hermitian factor, full coefficients.
 * @param pivots Matching raw classic signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hpcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<float>> factors,
      RawLapackPivotView pivots, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex HPCON using Hermitian symmetry.
 * @param provider Explicit checked reference selection; unchanged.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed classic Hermitian factor; entries unread.
 * @param pivots Matching raw classic block pivots, exact n; entries unread.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host output; unread.
 * @return Complete formula plan or structural error; no foreign call/writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHpconWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, double original_norm,
    const double& reciprocal_condition);

/** @brief Estimates double complex Hermitian reciprocal condition.
 * @param provider Explicit same-build reference provider; no fallback.
 * @param triangle Selected upper/lower factor triangle.
 * @param factors Immutable packed classic Hermitian factor, full coefficients.
 * @param pivots Matching raw classic signed paired pivots, exact n.
 * @param original_norm Finite nonnegative original-matrix one-norm.
 * @param reciprocal_condition Disjoint live real host RCOND output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit live disjoint scalar/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hpcon(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<const std::complex<double>> factors,
      RawLapackPivotView pivots, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_CONDITION_H_
