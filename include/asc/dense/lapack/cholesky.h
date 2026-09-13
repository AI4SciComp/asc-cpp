#ifndef ASC_DENSE_LAPACK_CHOLESKY_H_
#define ASC_DENSE_LAPACK_CHOLESKY_H_

/** @file
 * @brief Provider-free selected-triangle Cholesky and reusable solves.
 *
 * POTRF computes A=L*L^H from the lower triangle or A=U^H*U from the upper
 * triangle, using transpose for real scalars. Only the selected triangle is
 * referenced or overwritten. Complex diagonal imaginary components are ignored
 * on input and replaced with positive zero. No blanket symmetry or finiteness
 * scan is performed; success is not a conditioning/finiteness guarantee.
 *
 * The first nonpositive or NaN real Schur-complement pivot stops factorization:
 * its unsquared real value replaces the diagonal, earlier factor columns
 * (lower) or rows (upper) remain available, and later selected entries retain
 * their original values. The report returns kNotPositiveDefinite with
 * kDocumentedPartial and the zero-based pivot index. Such output cannot become
 * a successful LapackCholeskyFactorView.
 *
 * POTRS overwrites B with X in A*X=B, reusing an unmodified successful native
 * factor. Factor diagonal positivity, finiteness and real-valuedness are
 * checked before any RHS write when n and nrhs are nonzero. A failure leaves B
 * unchanged. Both layouts and valid leading dimensions are traversed directly;
 * ignored triangle and padding bytes are untouched.
 *
 * Calls require serial CPU execution and kHost storage. Pinned, device and
 * managed placements follow the existing serial-context rejection policy. Both
 * have exactly zero scratch requirements, allocate nothing, and perform no
 * transfer, synchronization, provider dispatch or Random operation. Reports
 * are initialized before ordinary validation; report/storage aliases are
 * rejected without resetting the aliased report. Native INFO remains absent
 * and called_provider remains false. Structural failure leaves numerical
 * storage unchanged. Empty operations do not access numeric elements.
 * Independent disjoint calls are reentrant; callers retain all borrowed
 * lifetimes and exclude concurrent mutation.
 */

#include <complex>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/export.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"

namespace asc {

/** @brief Factors a real single-precision selected triangle in place.
 * @param context Explicit serial CPU execution.
 * @param triangle Upper or lower Hermitian/symmetric interpretation.
 * @param matrix Mutable square caller storage; see the file contract.
 * @param report Mandatory failure-surviving diagnostics, disjoint from matrix.
 * @return OK, structural failure, or kNumerical at a nonpositive/NaN pivot.
 * @note O(n^3) arithmetic and O(1) auxiliary storage.
 */
ASC_DENSE_EXPORT Status Potrf(const ExecutionContext& context,
                              DenseBlasTriangle triangle,
                              DenseBlasMatrixView<float> matrix,
                              LapackReport& report);
/** @brief Factors a real double-precision selected triangle in place.
 * @param context Explicit serial CPU execution.
 * @param triangle Upper or lower Hermitian/symmetric interpretation.
 * @param matrix Mutable square caller storage; see the file contract.
 * @param report Mandatory failure-surviving diagnostics, disjoint from matrix.
 * @return OK, structural failure, or kNumerical at a nonpositive/NaN pivot.
 * @note O(n^3) arithmetic and O(1) auxiliary storage.
 */
ASC_DENSE_EXPORT Status Potrf(const ExecutionContext& context,
                              DenseBlasTriangle triangle,
                              DenseBlasMatrixView<double> matrix,
                              LapackReport& report);
/** @brief Factors a complex single-precision selected Hermitian triangle.
 * @param context Explicit serial CPU execution.
 * @param triangle Upper or lower interpretation; not complex symmetry.
 * @param matrix Mutable square storage; imaginary diagonals are ignored.
 * @param report Mandatory failure-surviving diagnostics, disjoint from matrix.
 * @return OK, structural failure, or kNumerical at a nonpositive/NaN pivot.
 * @note O(n^3) arithmetic and O(1) auxiliary storage.
 */
ASC_DENSE_EXPORT Status Potrf(const ExecutionContext& context,
                              DenseBlasTriangle triangle,
                              DenseBlasMatrixView<std::complex<float>> matrix,
                              LapackReport& report);
/** @brief Factors a complex double-precision selected Hermitian triangle.
 * @param context Explicit serial CPU execution.
 * @param triangle Upper or lower interpretation; not complex symmetry.
 * @param matrix Mutable square storage; imaginary diagonals are ignored.
 * @param report Mandatory failure-surviving diagnostics, disjoint from matrix.
 * @return OK, structural failure, or kNumerical at a nonpositive/NaN pivot.
 * @note O(n^3) arithmetic and O(1) auxiliary storage.
 */
ASC_DENSE_EXPORT Status Potrf(const ExecutionContext& context,
                              DenseBlasTriangle triangle,
                              DenseBlasMatrixView<std::complex<double>> matrix,
                              LapackReport& report);

/** @brief Solves with a successful native real single-precision factor.
 * @param context Explicit serial CPU execution.
 * @param factor Unmodified successful native factor with selected triangle.
 * @param rhs Mutable n-by-nrhs B/X, disjoint from all factor storage.
 * @param report Mandatory diagnostics, disjoint from factor/RHS storage.
 * @return OK, structural failure, or unchanged kNumerical for invalid diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see the file contract.
 */
ASC_DENSE_EXPORT Status Potrs(const ExecutionContext& context,
                              LapackCholeskyFactorView<float> factor,
                              DenseBlasMatrixView<float> rhs,
                              LapackReport& report);
/** @brief Solves with a successful native real double-precision factor.
 * @param context Explicit serial CPU execution.
 * @param factor Unmodified successful native factor with selected triangle.
 * @param rhs Mutable n-by-nrhs B/X, disjoint from all factor storage.
 * @param report Mandatory diagnostics, disjoint from factor/RHS storage.
 * @return OK, structural failure, or unchanged kNumerical for invalid diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see the file contract.
 */
ASC_DENSE_EXPORT Status Potrs(const ExecutionContext& context,
                              LapackCholeskyFactorView<double> factor,
                              DenseBlasMatrixView<double> rhs,
                              LapackReport& report);
/** @brief Solves with a successful native complex single-precision factor.
 * @param context Explicit serial CPU execution.
 * @param factor Unmodified successful native Hermitian factor.
 * @param rhs Mutable n-by-nrhs B/X, disjoint from all factor storage.
 * @param report Mandatory diagnostics, disjoint from factor/RHS storage.
 * @return OK, structural failure, or unchanged kNumerical for invalid diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see the file contract.
 */
ASC_DENSE_EXPORT Status
Potrs(const ExecutionContext& context,
      LapackCholeskyFactorView<std::complex<float>> factor,
      DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report);
/** @brief Solves with a successful native complex double-precision factor.
 * @param context Explicit serial CPU execution.
 * @param factor Unmodified successful native Hermitian factor.
 * @param rhs Mutable n-by-nrhs B/X, disjoint from all factor storage.
 * @param report Mandatory diagnostics, disjoint from factor/RHS storage.
 * @return OK, structural failure, or unchanged kNumerical for invalid diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see the file contract.
 */
ASC_DENSE_EXPORT Status
Potrs(const ExecutionContext& context,
      LapackCholeskyFactorView<std::complex<double>> factor,
      DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_CHOLESKY_H_
