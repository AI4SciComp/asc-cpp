#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_H_

/** @file
 * @brief Explicit Reference-LAPACK positive-definite band factors and solves.
 *
 * PBTRF and PBTF2 overwrite selected band entries with U for A=U^H*U or L
 * for A=L*L^H, using transpose for real scalars. Upper/lower and row/column
 * storage follow LapackPositiveDefiniteBandView. Padding and unused band
 * corners are not read or written. Complex diagonal imaginary inputs are
 * not read, including during row packing; factorization publishes the exact
 * source-defined
 * partial band, including any trailing entries not yet factored, on positive
 * INFO. PBTRF chooses its pinned upstream blocked/unblocked algorithm; PBTF2
 * calls the distinct unblocked routine. Neither has a foreign WORK query.
 * Partial row publication writes an imaginary diagonal component only where
 * the source factor/update path overwrites it; untouched trailing imaginary
 * components remain unwritten, without being copied through workspace.
 *
 * PBTRS consumes a raw, unmodified band triangular factor and overwrites B
 * with X in A*X=B. The caller supplies valid factor provenance; this raw
 * expert API neither promotes failed factors to a successful factor object
 * nor checks/normalizes the factor diagonal. All complex factor components,
 * including imaginary diagonals, retain their upstream triangular meaning.
 * RHS and band layouts are independent. No refactorization or inverse occurs.
 *
 * Queries compute checked storage formulas without reading values or calling
 * the provider. Column-major operands need no conversion space. Row-major
 * band conversion uses n*(kd+1) live scalar objects in kLayoutConversion;
 * row-major RHS adds n*nrhs objects when the solve is nonempty. Execution
 * uses only that explicit caller storage, never an allocating LAPACKE branch
 * or a densified n*n representation. All supplied workspace regions must be
 * disjoint, correctly aligned and accessible through the explicit serial
 * provider; unused zero-byte region metadata retains neutral validation.
 * Shape, storage, source integer intermediates, context, aliases and exact
 * plan identity/capacities are checked before numerical mutation/calls.
 *
 * Reports preserve raw signed INFO. Factor positive INFO=i returns kNumerical,
 * kNotPositiveDefinite, zero-based diagnostic i-1 and kDocumentedPartial.
 * Negative, impossible or unwritten INFO returns kProvider with unusable
 * output; packed output is not published, while directly supplied column
 * storage cannot be rolled back after an unexpected foreign failure. Successful
 * execution is not a finite-quality guarantee: pinned PBTF2 does not detect
 * NaN by its AJJ<=0 test, and PBTRS reports no numerical INFO. Such raw results
 * are not credited as mathematical success. No blanket finiteness/symmetry
 * scan or artificial pivot tolerance is imposed.
 *
 * Calls allocate nothing in ASC, transfer/synchronize nothing, and never
 * change provider or precision. The pinned provider's static allocation
 * closure and scoped probes are recorded separately in the program review.
 * Each operation borrows live disjoint storage for its synchronous call.
 * Independent calls are reentrant; callers exclude overlapping mutation.
 * Reports reset before ordinary validation. Metadata/storage aliases are
 * rejected without resetting an aliased report. Preflight failures leave
 * numerical storage unchanged, with called_provider=false and absent INFO.
 * Empty valid execution uses safe dummy pointers for the real upstream
 * quick return; it reads/writes no numerical elements and retains actual INFO.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries checked PBTRF band-conversion storage for float.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable selected band; no values are read.
 * @return Immutable-shape/provider-bound plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix);
/** @brief Queries checked PBTRF band-conversion storage for double.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable selected band; no values are read.
 * @return Immutable-shape/provider-bound plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix);
/** @brief Queries checked PBTRF band-conversion storage for complex float.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable Hermitian band; no values are read.
 * @return Immutable-shape/provider-bound plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix);
/** @brief Queries checked PBTRF band-conversion storage for complex double.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable Hermitian band; no values are read.
 * @return Immutable-shape/provider-bound plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix);

/** @brief Queries checked PBTF2 band-conversion storage for float.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable selected band; no values are read.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix);
/** @brief Queries checked PBTF2 band-conversion storage for double.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable selected band; no values are read.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix);
/** @brief Queries checked PBTF2 band-conversion storage for complex float.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable Hermitian band; no values are read.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix);
/** @brief Queries checked PBTF2 band-conversion storage for complex double.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Borrowed mutable Hermitian band; no values are read.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix);

/** @brief Queries independent band/RHS layout-conversion storage for float.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Borrowed raw const triangular band, with caller provenance.
 * @param rhs Mutable n-by-nrhs B/X; disjoint from factor, either layout.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> factor,
    DenseBlasMatrixView<float> rhs);
/** @brief Queries independent band/RHS layout-conversion storage for double.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Borrowed raw const triangular band, with caller provenance.
 * @param rhs Mutable n-by-nrhs B/X; disjoint from factor, either layout.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> factor,
    DenseBlasMatrixView<double> rhs);
/** @brief Queries independent band/RHS conversion storage for complex float.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Raw const triangular band; all complex components are retained.
 * @param rhs Mutable n-by-nrhs B/X; disjoint from factor, either layout.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Queries independent band/RHS conversion storage for complex double.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Raw const triangular band; all complex components are retained.
 * @param rhs Mutable n-by-nrhs B/X; disjoint from factor, either layout.
 * @return Routine/shape/provider-bound formula plan or preflight failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbtrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Executes SPBTRF on a selected positive-definite band.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; padding and corners remain untouched.
 * @param plan Exact unchanged QueryPbtrfWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory failure-surviving raw INFO/partial-output report.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<float> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes DPBTRF on a selected positive-definite band.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; padding and corners remain untouched.
 * @param plan Exact unchanged QueryPbtrfWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory failure-surviving raw INFO/partial-output report.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<double> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes CPBTRF on a selected Hermitian positive-definite band.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; imaginary input diagonals are ignored.
 * @param plan Exact unchanged QueryPbtrfWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory failure-surviving raw INFO/partial-output report.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes ZPBTRF on a selected Hermitian positive-definite band.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; imaginary input diagonals are ignored.
 * @param plan Exact unchanged QueryPbtrfWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory failure-surviving raw INFO/partial-output report.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Executes the distinct unblocked SPBTF2 factorization.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; padding and corners remain untouched.
 * @param plan Exact unchanged QueryPbtf2Workspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO/partial-output report, including NaN caveat.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtf2(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<float> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes the distinct unblocked DPBTF2 factorization.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; padding and corners remain untouched.
 * @param plan Exact unchanged QueryPbtf2Workspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO/partial-output report, including NaN caveat.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtf2(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<double> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes the distinct unblocked CPBTF2 Hermitian factorization.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; imaginary input diagonals are ignored.
 * @param plan Exact unchanged QueryPbtf2Workspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO/partial-output report, including NaN caveat.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtf2(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes the distinct unblocked ZPBTF2 Hermitian factorization.
 * @param provider Explicit pinned serial reference provider.
 * @param matrix Mutable selected band; imaginary input diagonals are ignored.
 * @param plan Exact unchanged QueryPbtf2Workspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO/partial-output report, including NaN caveat.
 * @return OK, preflight failure, kNumerical or unexpected kProvider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtf2(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Executes SPBTRS with raw const band factors and multiple RHS.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Raw selected triangular band; caller retains factor provenance.
 * @param rhs Mutable n-by-nrhs B/X, independently laid out and disjoint.
 * @param plan Exact unchanged QueryPbtrsWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO; the source has no positive numerical INFO.
 * @return OK or structural/provider failure; no diagonal predicate is imposed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrs(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const float> factor,
      DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Executes DPBTRS with raw const band factors and multiple RHS.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Raw selected triangular band; caller retains factor provenance.
 * @param rhs Mutable n-by-nrhs B/X, independently laid out and disjoint.
 * @param plan Exact unchanged QueryPbtrsWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO; the source has no positive numerical INFO.
 * @return OK or structural/provider failure; no diagonal predicate is imposed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrs(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const double> factor,
      DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Executes CPBTRS, retaining every raw complex factor component.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Raw selected triangular band; imaginary diagonals are retained.
 * @param rhs Mutable n-by-nrhs B/X, independently laid out and disjoint.
 * @param plan Exact unchanged QueryPbtrsWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO; the source has no positive numerical INFO.
 * @return OK or structural/provider failure; no diagonal predicate is imposed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrs(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<float>> factor,
      DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Executes ZPBTRS, retaining every raw complex factor component.
 * @param provider Explicit pinned serial reference provider.
 * @param factor Raw selected triangular band; imaginary diagonals are retained.
 * @param rhs Mutable n-by-nrhs B/X, independently laid out and disjoint.
 * @param plan Exact unchanged QueryPbtrsWorkspace result.
 * @param workspace Disjoint live caller buffers satisfying the plan.
 * @param report Mandatory raw INFO; the source has no positive numerical INFO.
 * @return OK or structural/provider failure; no diagonal predicate is imposed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbtrs(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<double>> factor,
      DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_H_
