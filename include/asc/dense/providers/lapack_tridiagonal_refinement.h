#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_REFINEMENT_H_

/** @file
 * @brief Explicit reference general-tridiagonal iterative refinement.
 *
 * The lifetime, context, alias, report, allocation and plan contracts of
 * lapack_tridiagonal.h apply. No full matrix is formed and no global provider
 * selection occurs. All referenced arrays are disjoint contiguous CPU storage.
 * Queries read metadata and scalar options only; execution checks borrowed
 * adjacent pivot values after context admission. Raw factors and pivots must
 * originate together; validation cannot prove numerical provenance. No factor
 * report is fabricated, and report.factor_family remains absent.
 * GTRFS accepts all N/T/C operations and independent B/X layouts. Original
 * DL/D/DU, raw factor arrays, pivots and B remain immutable. X is an initial
 * solution updated in place. Active calls reject exactly zero factor D before
 * any output or workspace writes. No blanket finite-input guarantee is made.
 * FERR/BERR are exact-nrhs real output vectors, increment one. Estimates use
 * the pinned algorithm, including its complex componentwise magnitude rules;
 * they are not rigorous certificates. Negative diagnostics are provider
 * defects; nonfinite diagnostics remain visible with accuracy-warning partial
 * validity. Real workspace is 3*n scalars and 2*n provider kInteger entries:
 * first n pivots, then a distinct simultaneous n-entry estimator array.
 * Complex uses 2*n scalars, n real entries and n provider kInteger pivots.
 * Each row-major B or X requires n*nrhs additional live layout
 * scalars. Empty n or nrhs completes locally, setting any FERR/BERR entries to
 * zero; no factor or pivot entry is read. Structural failures leave all
 * numerical data unchanged.
 * The pinned source can return infinite FERR for the exactly solved singleton
 * A=B=min_normal/8, X=1. ASC retains that diagnostic and reports an accuracy
 * warning. This required extreme-scaling mathematical gate remains incomplete;
 * it is not repaired by substituting a different refinement algorithm.
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
#include "asc/dense/providers/lapack_tridiagonal.h"

namespace asc {

/** @brief Checks single real GTRFS metadata and exact scratch.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @return A metadata-bound fixed-capacity plan or a structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Refines single real solutions and estimates error bounds.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned typed, layout and separate provider-integer
 * regions.
 * @param report Mandatory report, reset before ordinary structural preflight.
 * @return OK, structural/numerical failure, or provider defect; raw INFO
 * survives.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtrfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double real GTRFS metadata and exact scratch.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @return A metadata-bound fixed-capacity plan or a structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Refines double real solutions and estimates error bounds.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned typed, layout and separate provider-integer
 * regions.
 * @param report Mandatory report, reset before ordinary structural preflight.
 * @return OK, structural/numerical failure, or provider defect; raw INFO
 * survives.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtrfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks single complex GTRFS metadata and exact scratch.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @return A metadata-bound fixed-capacity plan or a structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Refines single complex solutions and estimates error bounds.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned typed, layout and separate provider-integer
 * regions.
 * @param report Mandatory report, reset before ordinary structural preflight.
 * @return OK, structural/numerical failure, or provider defect; raw INFO
 * survives.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtrfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double complex GTRFS metadata and exact scratch.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @return A metadata-bound fixed-capacity plan or a structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Refines double complex solutions and estimates error bounds.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original contiguous tridiagonal coefficients.
 * @param factors Immutable same-matrix raw LU and DU2.
 * @param pivots Same-factor nominal adjacent pivots.
 * @param rhs Immutable n-by-nrhs right-hand sides, either layout.
 * @param solution Mutable initial n-by-nrhs solution, independently chosen
 * layout.
 * @param forward_error Live exact-nrhs real FERR vector; query does not read or
 * write it.
 * @param backward_error Live disjoint exact-nrhs real BERR vector.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned typed, layout and separate provider-integer
 * regions.
 * @param report Mandatory report, reset before ordinary structural preflight.
 * @return OK, structural/numerical failure, or provider defect; raw INFO
 * survives.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtrfs(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_REFINEMENT_H_
