#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_REFINEMENT_H_

/** @file
 * @brief Explicit classic indefinite iterative refinement and error estimates.
 *
 * SYRFS refines A*X=B with transpose-based real/complex symmetric factors;
 * HERFS uses Hermitian factors. A, AF and B are immutable; X is an input
 * approximation improved in place. The caller guarantees that the selected
 * original A, same-provider classic factor AF and raw signed paired pivots
 * have matching mathematical provenance. These raw inputs do not themselves
 * certify success. No factorization or implicit preservation copy occurs.
 *
 * Only selected A/AF triangles are referenced. Original Hermitian A diagonals
 * use real components; AF retains actual complete complex block coefficients.
 * All four matrix layouts are independent. Row-major A/AF/B/X consume their
 * full logical counts in explicit kLayoutConversion storage, in that order;
 * selected-triangle packing never reads the unused triangle. Column-major
 * remains direct. Only logical X output is published, never padding.
 *
 * Active real calls need 3*n live scalar WORK entries and 2*n provider-width
 * kInteger entries: converted IPIV [0,n), simultaneous IWORK [n,2*n).
 * Active complex calls need 2*n live scalar WORK, n underlying-real kReal,
 * and n provider-width kInteger pivot entries. Integer lifetimes begin in
 * caller byte storage; scalar/real/layout objects are already live. Source
 * LACN2 3*n, RFS n+1 and terminal NRHS+1 arithmetic are checked separately.
 *
 * Formula queries read only metadata, binding source routine/scalar/triangle,
 * every shape/layout/original and effective stride, vector lengths/increments
 * and exact provider build/ABI. Unused one-column strides normalize to the
 * row count while original strides remain in the key. Exact classic paired
 * pivots and evaluated-zero block divisors are checked before any writes.
 * Zero divisors return kNumerical/kSingular and a zero-based block index,
 * with absent native INFO; no blanket finiteness scan is added.
 *
 * Structural failures preserve all numerical destinations and do not enter
 * LAPACK. Reports reset with absent INFO/called_provider=false except unsafe
 * metadata/report aliases, which leave even the report untouched. Empty n
 * or nrhs is a successful noncall: FERR/BERR are zero and X unchanged, with
 * no scratch requirement and no A/AF/pivot/B/X entry reads.
 *
 * INFO=0 preserves improved X and error diagnostics. FERR estimates relative
 * maximum-component forward error, not a guaranteed bound. BERR is the
 * componentwise backward-error diagnostic with upstream safe-minimum terms.
 * Complex diagnostic component magnitudes use abs(real)+abs(imag), except
 * original Hermitian diagonals use abs(real). Nonfinite diagnostics produce
 * a retained accuracy warning; negative diagnostics are provider-invalid.
 * Nonzero foreign INFO is a provider defect retained verbatim with unusable
 * output; packed X is withheld, but direct column-major X may have changed.
 * No finiteness guarantee is added for arbitrary inputs or intermediate
 * overflow merely because raw INFO is zero.
 *
 * Calls are synchronous serial CPU operations of the explicit provider.
 * No hidden allocation, transfer, synchronization, fallback or global state
 * change occurs. All operands and nonempty scratch must be accessible to the
 * context. Numerical buffers, scratch and live metadata are disjoint and
 * live through the call; concurrent calls require separate writable storage.
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

/** @brief Queries single real SYRFS without numerical reads or foreign calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A.
 * @param factors Immutable matching classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines a single real symmetric indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A.
 * @param factors Immutable matching classic AF.
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
ASC_DENSE_LAPACK_EXPORT Status Syrfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYRFS without numerical reads or foreign calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A.
 * @param factors Immutable matching classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines a double real symmetric indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A.
 * @param factors Immutable matching classic AF.
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
ASC_DENSE_LAPACK_EXPORT Status Syrfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex transpose-based SYRFS without numerical reads.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original square A, including complete diagonals.
 * @param factors Immutable matching classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines a single complex symmetric, not Hermitian, solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original square A, including complete diagonals.
 * @param factors Immutable matching classic AF.
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
ASC_DENSE_LAPACK_EXPORT Status Syrfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex transpose-based SYRFS without numerical reads.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original square A, including complete diagonals.
 * @param factors Immutable matching classic AF.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySyrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines a double complex symmetric, not Hermitian, solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param original Immutable original square A, including complete diagonals.
 * @param factors Immutable matching classic AF.
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
ASC_DENSE_LAPACK_EXPORT Status Syrfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex HERFS without numerical reads or foreign
 * calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original square A; imaginary diagonals ignored.
 * @param factors Immutable matching classic AF with full block coefficients.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Refines a single complex Hermitian indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original square A; imaginary diagonals ignored.
 * @param factors Immutable matching classic AF with full block coefficients.
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
ASC_DENSE_LAPACK_EXPORT Status Herfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex HERFS without numerical reads or foreign
 * calls.
 * @param provider Explicit checked reference selection.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original square A; imaginary diagonals ignored.
 * @param factors Immutable matching classic AF with full block coefficients.
 * @param pivots Raw same-call classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs B.
 * @param solution Mutable n-by-nrhs approximation X, unread during query.
 * @param forward_error Contiguous nrhs-entry underlying-real FERR output.
 * @param backward_error Contiguous nrhs-entry underlying-real BERR output.
 * @return Complete metadata-bound plan or structural failure; no writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHerfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Refines a double complex Hermitian indefinite solution.
 * @param provider Explicit same-build reference provider.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param original Immutable original square A; imaginary diagonals ignored.
 * @param factors Immutable matching classic AF with full block coefficients.
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
ASC_DENSE_LAPACK_EXPORT Status Herfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_REFINEMENT_H_
