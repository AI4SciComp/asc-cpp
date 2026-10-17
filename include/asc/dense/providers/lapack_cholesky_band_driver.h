#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_DRIVER_H_

/** @file
 * @brief Explicit Reference-LAPACK positive-definite band simple drivers.
 *
 * PBSV calls the actual pinned driver: PBTRF followed, only on successful
 * factorization, by PBTRS. It factors A even for zero RHS. Upper means
 * A=U^H U, lower means A=L L^H (transpose for real). Only the selected band
 * is read or written; Hermitian input diagonal imaginary components are
 * ignored according to PBTRF, not a full-matrix symmetry/finite scan.
 *
 * Band and RHS layouts are independent. Column-major needs no numerical
 * workspace; row-major A requires n*(kd+1) live T entries and row-major B
 * requires n*nrhs further entries in kLayoutConversion. This is band packing,
 * not densification, and never allocation, transfer, hidden fallback or
 * synchronization. Formula queries make no foreign call or numerical reads.
 * Caller storage, provider, plan, workspace and report are disjoint and live
 * throughout a call. Context admission is checked for every nonempty workspace
 * region, including unused roles; neutral zero-byte compatibility is retained.
 *
 * Structural failures do not call the provider or change numerical storage.
 * The report is reset first except when aliased metadata prevents a safe reset.
 * INFO>0 through n retains selected partial A, leaves B unchanged, and reports
 * a non-positive leading minor with zero-based diagnostic_index. INFO=0
 * publishes A/B but is raw provider success, not a finiteness guarantee.
 * Complex factor packing reads only real diagonal input components. Partial
 * row publication writes imaginary diagonals only where the exact pinned
 * PBTRF route normalizes them; untouched trailing components remain unwritten.
 * Negative or impossible INFO returns kProvider/unusable; packed row outputs
 * are not published, while direct column-major mutations cannot be rolled back.
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

/** @brief Computes exact checked PBSV caller packing capacities.
 * @param provider Explicit reference selection; no foreign query is performed.
 * @param matrix Mutable positive-definite band A descriptor, unchanged.
 * @param rhs Mutable n-by-nrhs RHS descriptor in either layout, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> matrix,
    DenseBlasMatrixView<float> rhs);
/** @brief Factors a positive-definite band matrix and solves all RHS.
 * @param provider Explicit checked reference provider and execution context.
 * @param matrix Selected band A overwritten by its factor or partial factor.
 * @param rhs B overwritten by X only after successful factorization.
 * @param plan Unmodified matching formula plan; execution does not query
 * upstream.
 * @param workspace Caller-owned disjoint live scalar packing objects.
 * @param report Mandatory failure-surviving raw INFO and output-validity
 * report.
 * @return OK, structural failure, kNumerical for a non-positive leading minor,
 * or kProvider for an invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteBandView<float> matrix,
     DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
     const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact checked PBSV caller packing capacities.
 * @param provider Explicit reference selection; no foreign query is performed.
 * @param matrix Mutable positive-definite band A descriptor, unchanged.
 * @param rhs Mutable n-by-nrhs RHS descriptor in either layout, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> matrix,
    DenseBlasMatrixView<double> rhs);
/** @brief Factors a positive-definite band matrix and solves all RHS.
 * @param provider Explicit checked reference provider and execution context.
 * @param matrix Selected band A overwritten by its factor or partial factor.
 * @param rhs B overwritten by X only after successful factorization.
 * @param plan Unmodified matching formula plan; execution does not query
 * upstream.
 * @param workspace Caller-owned disjoint live scalar packing objects.
 * @param report Mandatory failure-surviving raw INFO and output-validity
 * report.
 * @return OK, structural failure, kNumerical for a non-positive leading minor,
 * or kProvider for an invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteBandView<double> matrix,
     DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
     const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact checked PBSV caller packing capacities.
 * @param provider Explicit reference selection; no foreign query is performed.
 * @param matrix Mutable positive-definite band A descriptor, unchanged.
 * @param rhs Mutable n-by-nrhs RHS descriptor in either layout, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Factors a positive-definite band matrix and solves all RHS.
 * @param provider Explicit checked reference provider and execution context.
 * @param matrix Selected band A overwritten by its factor or partial factor.
 * @param rhs B overwritten by X only after successful factorization.
 * @param plan Unmodified matching formula plan; execution does not query
 * upstream.
 * @param workspace Caller-owned disjoint live scalar packing objects.
 * @param report Mandatory failure-surviving raw INFO and output-validity
 * report.
 * @return OK, structural failure, kNumerical for a non-positive leading minor,
 * or kProvider for an invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteBandView<std::complex<float>> matrix,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Computes exact checked PBSV caller packing capacities.
 * @param provider Explicit reference selection; no foreign query is performed.
 * @param matrix Mutable positive-definite band A descriptor, unchanged.
 * @param rhs Mutable n-by-nrhs RHS descriptor in either layout, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Factors a positive-definite band matrix and solves all RHS.
 * @param provider Explicit checked reference provider and execution context.
 * @param matrix Selected band A overwritten by its factor or partial factor.
 * @param rhs B overwritten by X only after successful factorization.
 * @param plan Unmodified matching formula plan; execution does not query
 * upstream.
 * @param workspace Caller-owned disjoint live scalar packing objects.
 * @param report Mandatory failure-surviving raw INFO and output-validity
 * report.
 * @return OK, structural failure, kNumerical for a non-positive leading minor,
 * or kProvider for an invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteBandView<std::complex<double>> matrix,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_DRIVER_H_
