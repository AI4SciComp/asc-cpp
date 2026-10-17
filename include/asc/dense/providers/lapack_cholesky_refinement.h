#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_REFINEMENT_H_

/** @file
 * @brief Explicit reference iterative refinement for positive-definite systems.
 *
 * PORFS refines an existing X using original symmetric/Hermitian A, its
 * matching selected Cholesky factor AF and original B. A/AF/B are borrowed
 * unchanged, with caller-guaranteed common provenance. A's ignored triangle
 * and complex imaginary diagonal are not read; AF's actual triangular
 * coefficients, including its complex diagonal, are meaningful input.
 *
 * All four matrix layouts are independent. Active real WORK has 3*n live
 * scalar entries and n ABI-width kInteger entries; complex WORK has 2*n live
 * scalar entries and n underlying-real kReal entries. Both paths check the
 * source LACN2 expression 3*n. Each row-major matrix adds its full logical
 * extent count of live scalar kLayoutConversion objects; A/AF pack only the
 * selected triangle, A with real-only diagonal. B and existing X pack all
 * logical entries. No allocating or implicit preservation convenience occurs.
 *
 * Formula queries read metadata only and bind every shape, layout, original
 * ASC stride, effective foreign stride, triangle, vector length/increment,
 * scalar and provider build/ABI. All storage must be accessible to the serial
 * provider. A/AF/B/X, real FERR/BERR and workspace are pairwise disjoint.
 * Lifetimes cover the call; reentrant calls use disjoint mutable storage.
 * No hidden allocation, transfer, synchronization or fallback occurs.
 *
 * Structural failures leave all numerical storage unchanged, with no provider
 * call or raw INFO. Metadata aliases leave the report untouched; otherwise it
 * resets before preflight. Exact zero AF diagonals fail before writes with
 * kNumerical/kSingular, absent INFO and the exact zero-based diagonal index.
 * This is not a positive-definiteness or finiteness scan. Empty n or nrhs
 * requires no scratch/input reads and sets FERR/BERR to zero without a
 * foreign call. An empty RHS does not inspect otherwise-unused factor values.
 *
 * Pinned PORFS documents no positive INFO; any nonzero INFO is a provider
 * defect with exact raw INFO and unusable output. INFO=0 publishes the
 * refined X and finite nonnegative FERR/BERR. FERR is an estimate, not a
 * universal upper bound. Complex error formulas use abs(real)+abs(imag),
 * unlike the Euclidean scalar modulus of the one-norm condition estimate.
 * Nonfinite error estimates retain completed X/raw diagnostics and produce
 * kNumerical/kAccuracyWarning with documented-partial validity. Negative
 * estimates are provider-invalid. No finite-X guarantee is added for arbitrary
 * nonfinite input, intermediate overflow or an unrepresentable solution.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real PORFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single real PORFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural/zero-factor failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Porfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real PORFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double real PORFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural/zero-factor failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Porfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex PORFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single complex PORFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural/zero-factor failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Porfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex PORFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPorfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double complex PORFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; selected Hermitian/symmetric input.
 * @param factors Immutable separate matching square Cholesky factor AF.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural/zero-factor failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Porfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_REFINEMENT_H_
