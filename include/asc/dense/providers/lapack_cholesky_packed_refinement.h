#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_REFINEMENT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_REFINEMENT_H_

/** @file
 * @brief Explicit reference refinement of ordinary packed positive-definite
 * systems.
 *
 * PPRFS refines an existing X using original symmetric/Hermitian packed AP,
 * its matching packed Cholesky factor AFP and original full B. AP/AFP/B are
 * borrowed unchanged, with caller-guaranteed common provenance. AP's complex
 * imaginary diagonal is not read; AFP's full triangular coefficients,
 * including the complex diagonal, are read as supplied. Raw factors carry
 * no positive-definiteness, zero-diagonal or finiteness certificate.
 *
 * All four layouts are independent. Active real WORK has 3*n live scalar
 * objects and n ABI-width kInteger entries; complex WORK has 2*n live scalar
 * objects and n underlying-real kReal entries. Each row packed AP/AFP adds
 * n*(n+1)/2 live scalar kLayoutConversion objects. AP packs a real-only
 * diagonal; AFP packs complete coefficients. Row B/X each add n*nrhs live
 * scalar objects. All packing is explicit caller storage; no densification,
 * allocation, transfer, synchronization, rescaling or fallback occurs.
 *
 * Formula queries read metadata only and bind routine, scalar, provider/ABI,
 * dimensions, layouts, both original/effective B/X strides and error vector
 * lengths/increments. All six operands and workspace are pairwise disjoint,
 * live and accessible to the serial provider. Reentrant calls require
 * disjoint mutable storage. The plan also checks actual pinned INTEGER
 * expressions, including 3*n and n*(n+1) before division.
 *
 * Structural or stale-plan failure preserves all numerical storage and makes
 * no foreign call. Metadata aliases preserve the report; otherwise it resets
 * before preflight. Empty n or nrhs needs no scratch or AP/AFP/B/X reads and
 * sets FERR/BERR to zero locally, with absent INFO and no unused foreign
 * dimension narrowing. Invalid flags and ASC metadata still fail.
 *
 * Active execution initializes INFO to signed MIN and FERR/BERR to NaN
 * immediately before calling PPRFS. Any nonzero, missing or partially written
 * INFO is a provider defect with exact signed raw INFO. Negative estimates
 * are provider defects. Defects preserve direct writes and withhold row X
 * publication. INFO0 finite nonnegative estimates completes; nonfinite
 * estimates retain completed X/raw diagnostics with kNumerical,
 * kAccuracyWarning and documented-partial validity. FERR is an estimate,
 * not a universal bound. Complex error formulas use abs(real)+abs(imag).
 * No finite-X guarantee is added for arbitrary nonfinite inputs, intermediate
 * overflow or unrepresentable solutions; no factor scan precedes execution.
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

/** @brief Queries single real PPRFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single real PPRFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Pprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real PPRFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double real PPRFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Pprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex PPRFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single complex PPRFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Pprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex PPRFS explicit workspace.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs input/output descriptor, unread.
 * @param forward_error Contiguous nrhs-entry real FERR output, unread.
 * @param backward_error Contiguous nrhs-entry real BERR output, unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPprfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double complex PPRFS iterative refinement.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; selected Hermitian/symmetric
 * input.
 * @param factors Immutable separate matching ordinary packed Cholesky factor
 * AFP.
 * @param rhs Immutable n-by-nrhs original right-hand sides.
 * @param solution Mutable n-by-nrhs existing X, refined in place.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving report; see metadata-alias
 * exception.
 * @return OK, structural failure, estimate warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Pprfs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_REFINEMENT_H_
