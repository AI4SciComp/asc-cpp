#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_REFINEMENT_H_

/** @file
 * @brief Checked packed indefinite refinement and forward/backward estimates.
 *
 * SPRFS refines A*X=B with transpose-based real/complex symmetric factors;
 * HPRFS uses Hermitian factors. Original packed AP, matching packed AFP,
 * classic signed paired IPIV and B are immutable. X is an initial solution
 * improved in place. Factors and exact-n pivots must come from the same
 * completed nonsingular SPTRF/HPTRF call of this provider and match AP/B/X.
 * Raw views do not certify provenance; no factorization occurs here.
 *
 * AP and AFP packed layouts and B/X dense layouts are independent. Row
 * operands consume their packed or logical counts in kLayoutConversion,
 * ordered AP, AFP, B, X. Original Hermitian AP diagonals use only real
 * components; AFP retains full factor coefficients. Column operands remain
 * direct. Only logical row X is published; padding remains untouched.
 *
 * Active real calls require 3*n scalar WORK and 2*n provider-width INTEGER
 * entries: converted IPIV followed by IWORK. Complex calls require 2*n
 * scalar WORK, n INTEGER pivots and n underlying-real RWORK. Both variants
 * additionally require 2*nrhs underlying-real entries for private FERR/BERR,
 * following complex RWORK in kReal. Error buffers are NaN-seeded only after
 * complete structural/pivot/divisor validation. All scalar/real/layout
 * objects are caller-owned and live; native INTEGER lifetimes begin in
 * caller byte storage. Full packed n*(n+1), compact TRS endpoints, LACN2
 * 3*n, RFS n+1 and native NRHS+1 are checked before native conversion.
 *
 * Queries read metadata only and bind provider/scalar, triangle/symmetry,
 * all orders/layouts, original and effective B/X strides, exact pivot count
 * and contiguous error-vector lengths/increments. Positive dimensions with
 * one RHS use compact native leading dimensions; original strides remain
 * in the key. Every paired/directional pivot and exactly zero evaluated
 * TRS block divisor is checked before mutable numerical operations. A zero
 * divisor returns numerical/singular with a zero-based block index and no
 * native INFO. No broad finiteness scan or CON-style zero-estimate path
 * is invented for singular factors.
 *
 * Structural failures preserve numerical buffers and scratch. Reports reset
 * unless unsafe live-metadata aliasing prevents even that write. Empty n or
 * nrhs is a successful noncall: live FERR/BERR entries become zero and X
 * remains unchanged, with no scratch or AP/AFP/IPIV/B/X value reads.
 *
 * Full-width INFO starts at its minimum integer value and must return zero;
 * private input pivots must remain unchanged. Otherwise caller FERR/BERR and
 * row X are preserved, while direct-column X may retain native effects.
 * After protocol validation actual FERR/BERR are published without clipping.
 * Nonnegative finite estimates accompany complete X; nonfinite estimates
 * retain a numerical accuracy warning and documented partial output.
 * Negative estimates are provider-invalid/unusable and withhold row X.
 * Negative infinity follows the negative-value rule. No new factor or
 * accuracy certificate is issued and INFO=0 does not guarantee finite X.
 *
 * FERR estimates relative maximum-component forward error; BERR is the
 * componentwise backward-error estimate including native safe-minimum terms.
 * Complex diagnostic magnitudes use abs(real)+abs(imag), while original
 * Hermitian diagonals use abs(real). The estimates are not guaranteed bounds.
 * Calls use the explicit serial CPU provider with no allocation, transfer,
 * fallback or global state. All numerical, scratch and live metadata ranges
 * are accessible and disjoint. Immutable inputs/provider/plans may be shared
 * concurrently with independent writable outputs/workspaces/reports.
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

/** @brief Queries single real SPRFS without numerical reads or foreign calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original packed square A.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines a single real symmetric indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original packed square A.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Independent-layout n-by-nrhs approximation, improved in
 * place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory diagnostics surviving failure; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SPRFS without numerical reads or foreign calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original packed square A.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines a double real symmetric indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original packed square A.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Independent-layout n-by-nrhs approximation, improved in
 * place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory diagnostics surviving failure; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex transpose-based SPRFS without numerical reads.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original packed square A, including complete
 * diagonals.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines a single complex symmetric, not Hermitian, solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original packed square A, including complete
 * diagonals.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Independent-layout n-by-nrhs approximation, improved in
 * place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory diagnostics surviving failure; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex transpose-based SPRFS without numerical reads.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original packed square A, including complete
 * diagonals.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines a double complex symmetric, not Hermitian, solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original packed square A, including complete
 * diagonals.
 * @param factors Immutable matching packed classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Independent-layout n-by-nrhs approximation, improved in
 * place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory diagnostics surviving failure; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex HPRFS without numerical reads or foreign
 * calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original packed square A; imaginary diagonals
 * ignored.
 * @param factors Immutable matching packed classic AF with full block
 * coefficients.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines a single complex Hermitian indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original packed square A; imaginary diagonals
 * ignored.
 * @param factors Immutable matching packed classic AF with full block
 * coefficients.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Independent-layout n-by-nrhs approximation, improved in
 * place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory diagnostics surviving failure; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Hprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex HPRFS without numerical reads or foreign
 * calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original packed square A; imaginary diagonals
 * ignored.
 * @param factors Immutable matching packed classic AF with full block
 * coefficients.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines a double complex Hermitian indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original packed square A; imaginary diagonals
 * ignored.
 * @param factors Immutable matching packed classic AF with full block
 * coefficients.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Independent-layout n-by-nrhs approximation, improved in
 * place.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory diagnostics surviving failure; see alias exception.
 * @return OK, structural/numerical failure, accuracy warning or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Hprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_REFINEMENT_H_
