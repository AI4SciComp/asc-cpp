#ifndef ASC_DENSE_LAPACK_LU_H_
#define ASC_DENSE_LAPACK_LU_H_

/** @file
 * @brief Allocation-free, provider-free partial-pivot LU and reusable solves.
 *
 * GETRF overwrites an m-by-n matrix with unit-lower L (implicit diagonal) and
 * upper U. Applying the recorded row swaps in increasing order to the original
 * matrix gives L*U. Every swap covers the whole row, including earlier L
 * multipliers. Raw pivots are signed, one-based sequential swaps, not a final
 * permutation. Complex pivot comparison uses abs(real)+abs(imag); first ties
 * win. No tolerance replaces the exact-zero diagonal test. Nonfinite operands
 * follow scalar arithmetic; success is not a finiteness/conditioning guarantee.
 *
 * GETRS consumes a successful square factor without modifying or refactorizing
 * it and overwrites B with X in op(A)*X=B. Both descriptor layouts and
 * arbitrary valid leading dimensions are operated on directly. Padding is
 * untouched. Both operations require serial execution and host storage, have
 * zero scratch requirements, allocate nothing, and perform no
 * transfer/synchronization or foreign-provider fallback. Independent disjoint
 * calls may execute concurrently; callers must keep buffers live and exclude
 * concurrent mutation.
 *
 * Reports are reset before validation. Structural failure leaves every numeric
 * buffer unchanged with kNotRun/kUnchanged. Native calls never set foreign INFO
 * or called_provider. A singular factorization completes the elimination but
 * returns kNumerical, kSingular/kDocumentedPartial and the first exact-zero
 * diagonal's zero-based diagnostic_index; the raw factors/pivots remain
 * available for inspection but cannot create a successful LapackLuFactorView. A
 * solve detecting an exact-zero diagonal rejects before changing B. Successful
 * empty operations produce complete reports without reading numeric elements.
 */

#include <complex>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/export.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"

namespace asc {

/** @brief Factors a real single-precision rectangular matrix in place.
 * @param context Explicit serial CPU execution context.
 * @param matrix Caller-owned mutable matrix, disjoint from pivots.
 * @param pivots Host output with size min(m,n) and increment one.
 * @param report Mandatory caller-owned diagnostics; see this file's contract.
 * @return OK, structural failure, or kNumerical for exact singularity.
 * @note O(m*n*min(m,n)) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrf(const ExecutionContext& context,
                              DenseBlasMatrixView<float> matrix,
                              DenseBlasVectorView<index_t> pivots,
                              LapackReport& report);
/** @brief Factors a real double-precision rectangular matrix in place.
 * @param context Explicit serial CPU execution context.
 * @param matrix Caller-owned mutable matrix, disjoint from pivots.
 * @param pivots Host output with size min(m,n) and increment one.
 * @param report Mandatory caller-owned diagnostics; see this file's contract.
 * @return OK, structural failure, or kNumerical for exact singularity.
 * @note O(m*n*min(m,n)) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrf(const ExecutionContext& context,
                              DenseBlasMatrixView<double> matrix,
                              DenseBlasVectorView<index_t> pivots,
                              LapackReport& report);
/** @brief Factors a complex single-precision rectangular matrix in place.
 * @param context Explicit serial CPU execution context.
 * @param matrix Caller-owned mutable matrix, disjoint from pivots.
 * @param pivots Host output with size min(m,n) and increment one.
 * @param report Mandatory caller-owned diagnostics; see this file's contract.
 * @return OK, structural failure, or kNumerical for exact singularity.
 * @note O(m*n*min(m,n)) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrf(const ExecutionContext& context,
                              DenseBlasMatrixView<std::complex<float>> matrix,
                              DenseBlasVectorView<index_t> pivots,
                              LapackReport& report);
/** @brief Factors a complex double-precision rectangular matrix in place.
 * @param context Explicit serial CPU execution context.
 * @param matrix Caller-owned mutable matrix, disjoint from pivots.
 * @param pivots Host output with size min(m,n) and increment one.
 * @param report Mandatory caller-owned diagnostics; see this file's contract.
 * @return OK, structural failure, or kNumerical for exact singularity.
 * @note O(m*n*min(m,n)) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrf(const ExecutionContext& context,
                              DenseBlasMatrixView<std::complex<double>> matrix,
                              DenseBlasVectorView<index_t> pivots,
                              LapackReport& report);

/** @brief Solves with a reusable native real single-precision square LU factor.
 * @param context Explicit serial CPU execution context.
 * @param transpose Selects A, transpose(A), or conjugate-transpose(A).
 * @param factor Successful unmodified native factor and its immutable pivots.
 * @param rhs Caller-owned n-by-nrhs B/X, disjoint from factors and pivots.
 * @param report Mandatory diagnostics; never borrows the factor's old report.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrs(const ExecutionContext& context,
                              DenseBlasTranspose transpose,
                              LapackLuFactorView<float> factor,
                              DenseBlasMatrixView<float> rhs,
                              LapackReport& report);
/** @brief Solves with a reusable native real double-precision square LU factor.
 * @param context Explicit serial CPU execution context.
 * @param transpose Selects A, transpose(A), or conjugate-transpose(A).
 * @param factor Successful unmodified native factor and its immutable pivots.
 * @param rhs Caller-owned n-by-nrhs B/X, disjoint from factors and pivots.
 * @param report Mandatory diagnostics; never borrows the factor's old report.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrs(const ExecutionContext& context,
                              DenseBlasTranspose transpose,
                              LapackLuFactorView<double> factor,
                              DenseBlasMatrixView<double> rhs,
                              LapackReport& report);
/** @brief Solves with a reusable native complex single-precision square LU.
 * @param context Explicit serial CPU execution context.
 * @param transpose Selects A, transpose(A), or conjugate-transpose(A).
 * @param factor Successful unmodified native factor and its immutable pivots.
 * @param rhs Caller-owned n-by-nrhs B/X, disjoint from factors and pivots.
 * @param report Mandatory diagnostics; never borrows the factor's old report.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrs(const ExecutionContext& context,
                              DenseBlasTranspose transpose,
                              LapackLuFactorView<std::complex<float>> factor,
                              DenseBlasMatrixView<std::complex<float>> rhs,
                              LapackReport& report);
/** @brief Solves with a reusable native complex double-precision square LU.
 * @param context Explicit serial CPU execution context.
 * @param transpose Selects A, transpose(A), or conjugate-transpose(A).
 * @param factor Successful unmodified native factor and its immutable pivots.
 * @param rhs Caller-owned n-by-nrhs B/X, disjoint from factors and pivots.
 * @param report Mandatory diagnostics; never borrows the factor's old report.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 * @note O(n*n*nrhs) arithmetic, O(1) auxiliary storage; see file contract.
 */
ASC_DENSE_EXPORT Status Getrs(const ExecutionContext& context,
                              DenseBlasTranspose transpose,
                              LapackLuFactorView<std::complex<double>> factor,
                              DenseBlasMatrixView<std::complex<double>> rhs,
                              LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_LU_H_
