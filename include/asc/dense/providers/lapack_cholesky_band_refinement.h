#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_REFINEMENT_H_

/** @file
 * @brief Explicit reference refinement for positive-definite band systems.
 *
 * PBRFS consumes separate original symmetric/Hermitian A, raw Cholesky AF,
 * original RHS B and an initial solution X. A and AF have matching order,
 * bandwidth and triangle but independent layouts; B and X also have
 * independent layouts. AF provenance is the caller's obligation, not a
 * descriptor certificate or an added diagonal predicate. Complex original A
 * diagonals use only their real components; every raw AF component is retained.
 *
 * Queries are checked formulas, not foreign LWORK calls. For n>0 and nrhs>0,
 * real WORK uses 3*n scalar plus n private ABI-width kInteger entries; complex
 * WORK uses 2*n scalar plus n underlying-real kReal entries. Each row-major
 * A/AF adds n*(kd+1), and each row-major B/X adds n*nrhs live scalar
 * kLayoutConversion entries, concatenated A, AF, B, X. No dense expansion of
 * the band, hidden allocation, transfer, synchronization or fallback occurs.
 * Caller scalar/real objects are live; execution starts trivial integer
 * lifetimes in the explicit checked byte workspace.
 *
 * n=0 or nrhs=0 requires no numerical/packing workspace and reads no A/AF/B/X
 * values. An actual provider call writes all nrhs FERR/BERR entries to zero.
 * FERR and BERR are disjoint contiguous underlying-real vectors of length
 * nrhs. FERR estimates relative forward error; BERR uses componentwise
 * backward error with the source safe-minimum guard. Complex error formulas
 * use CABS1 (absolute real plus absolute imaginary), not Euclidean modulus.
 *
 * All operands/workspace/metadata are disjoint. Every nonempty supplied
 * workspace role, including unused roles, passes the explicit context;
 * existing zero-byte compatibility is retained. Preflight is allocation-free,
 * leaves numerical outputs unchanged and avoids foreign entry. Unsafe
 * metadata alias rejection preserves the report instead of resetting it.
 * After INFO=0, row X is published; nonfinite/negative FERR or BERR returns
 * kNumerical with kAccuracyWarning and documented-partial validity while
 * preserving raw INFO, X and estimates. Finite estimates are not guaranteed
 * exact bounds, and INFO=0 is not a finite-X or factor-provenance promise.
 * The pinned provider can overflow its unscaled intermediate inverse solve
 * for tiny valid factors even when the weighted error bound is finite; that
 * case retains infinite FERR and reports the quality warning, not success.
 * Negative/impossible INFO is a provider defect; packed X is not published,
 * but direct column-major and estimate outputs cannot be rolled back.
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

/** @brief Computes checked PBRFS work and explicit layout capacities.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @return Identity-bound formula plan or structural/placement/ABI failure;
 * query makes no provider call and leaves all values unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Refines raw band-factor solutions and computes error estimates.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @param plan Unmodified matching fixed-formula plan.
 * @param workspace Caller-owned disjoint live numerical/packing buffers.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight failure, kNumerical estimate-quality warning or
 * kProvider defect; complete mutation/provenance semantics are above.
 */
ASC_DENSE_LAPACK_EXPORT Status Pbrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked PBRFS work and explicit layout capacities.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @return Identity-bound formula plan or structural/placement/ABI failure;
 * query makes no provider call and leaves all values unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Refines raw band-factor solutions and computes error estimates.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @param plan Unmodified matching fixed-formula plan.
 * @param workspace Caller-owned disjoint live numerical/packing buffers.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight failure, kNumerical estimate-quality warning or
 * kProvider defect; complete mutation/provenance semantics are above.
 */
ASC_DENSE_LAPACK_EXPORT Status Pbrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked PBRFS work and explicit layout capacities.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @return Identity-bound formula plan or structural/placement/ABI failure;
 * query makes no provider call and leaves all values unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Refines raw band-factor solutions and computes error estimates.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @param plan Unmodified matching fixed-formula plan.
 * @param workspace Caller-owned disjoint live numerical/packing buffers.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight failure, kNumerical estimate-quality warning or
 * kProvider defect; complete mutation/provenance semantics are above.
 */
ASC_DENSE_LAPACK_EXPORT Status Pbrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes checked PBRFS work and explicit layout capacities.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @return Identity-bound formula plan or structural/placement/ABI failure;
 * query makes no provider call and leaves all values unchanged.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbrfsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Refines raw band-factor solutions and computes error estimates.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original A; complex diagonal real-only.
 * @param factors Immutable raw matching band Cholesky factor AF.
 * @param rhs Immutable original B with n rows and nrhs columns.
 * @param solution Writable initial/refined X with the same shape as B.
 * @param forward_error Disjoint contiguous real FERR vector of length nrhs.
 * @param backward_error Disjoint contiguous real BERR vector of length nrhs.
 * @param plan Unmodified matching fixed-formula plan.
 * @param workspace Caller-owned disjoint live numerical/packing buffers.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight failure, kNumerical estimate-quality warning or
 * kProvider defect; complete mutation/provenance semantics are above.
 */
ASC_DENSE_LAPACK_EXPORT Status Pbrfs(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_REFINEMENT_H_
