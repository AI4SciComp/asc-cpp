#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_DRIVER_H_

/** @file
 * @brief Explicit positive-definite expert solves with three factor modes.
 *
 * Posvx selects FACT=N: original A/B are immutable; AF and X are separate
 * outputs. PosvxEquilibrated selects FACT=E: the provider computes S and may
 * replace selected A by diag(S)*A*diag(S) and B by diag(S)*B. The returned
 * equilibration describes actual scaling, not a request to force it.
 * PosvxFactored selects FACT=F: A must already have the supplied scaling and
 * AF must factor that A. A/AF/S are preserved; original B may be scaled.
 * Completed X solves the original unscaled system. No route fabricates
 * provenance or implicitly preserves overwritten originals.
 *
 * Only A's selected triangle and real diagonal components are inputs.
 * AF input in FACT=F includes the actual complex diagonal, not a Hermitian
 * diagonal normalization. All four layouts are independent. Row-major
 * matrices require their full logical scalar counts in kLayoutConversion,
 * packed in A/AF/B/X order. Complex A in FACT=N/E always requires that
 * packing, including column-major A: the source LACPY otherwise reads whole
 * complex diagonals. This mandatory scratch copies only selected components,
 * with real diagonals. Output-only AF/X packing never reads old output.
 * Packed A is published only in FACT=E when actual EQUED=Y. Ignored original
 * A imaginary diagonals are preserved if equilibration was not applied.
 *
 * Active real WORK uses 3*n live scalar entries plus n ABI-width kInteger
 * entries. Complex WORK uses 2*n scalar and n underlying-real kReal entries.
 * Both routes check the source LACN2 expression 3*n. Caller-owned packing,
 * scalar and real objects must be live. Foreign integer lifetimes are begun
 * in explicit aligned caller storage. No hidden allocation, transfer,
 * synchronization, fallback or handler change occurs.
 *
 * Formula queries bind mode, triangle, every shape, layout, original ASC
 * stride, effective foreign stride, vector metadata, selected supplied
 * equilibration and exact provider identity. They read no numerical entries.
 * Used supplied scales must be finite positive; unused FACT=F scales may
 * have length zero or n and are not read. All live operands, scalar outputs,
 * equilibration output and workspace must be disjoint and provider-accessible.
 * Lifetimes cover the call; concurrent calls use disjoint mutable storage.
 * Structural failure leaves numerical storage unchanged and calls no provider.
 * Metadata aliases leave report untouched; otherwise it resets before checks.
 * Supplied exact-zero AF diagonal fails before writes with absent INFO,
 * kNumerical/kSingular and the zero-based diagonal index; no broad scan occurs.
 *
 * Empty n requires no scratch/input reads: RCOND=1 and FERR/BERR=0, with no
 * foreign call; FACT=N/E actual equilibration is none. Empty nrhs does not
 * suppress nonempty factorization and condition estimation. Negative or
 * impossible INFO is a provider defect, with exact INFO and unusable output.
 * INFO in [1,n] is factorization failure: AF's selected partial coefficients,
 * any applied A/B scaling and RCOND=0 survive; X/FERR/BERR remain unchanged.
 * Row-major AF partial publication writes offdiagonals and real diagonal
 * components only, preserving old ignored imaginary components. This does
 * not promise byte equivalence with unspecified column-major imaginary
 * output. FACT=E scales can be partial after failed internal equilibration.
 *
 * INFO=n+1 is an accuracy warning, not exact singularity: the computed
 * solution and error estimates survive. RCOND describes equilibrated A.
 * FERR is an estimate, not a guaranteed bound; complex componentwise error
 * formulas use abs(real)+abs(imag), while condition estimates use the
 * Euclidean scalar modulus. Nonfinite diagnostics yield an accuracy warning;
 * negative diagnostics are provider-invalid. Raw outputs and INFO survive,
 * with only finite nonnegative individual diagnostics satisfying their
 * contract. No finite-X guarantee is added for arbitrary nonfinite inputs,
 * intermediate overflow or unrepresentable solutions.
 *
 * FACT=N/E reports identify Cholesky factors; only complete successful
 * reports authorize successful factor-view reuse. FACT=F does not certify
 * supplied raw factors. Failure reports never certify successful factors.
 */

#include <complex>
#include <cstdint>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Actual symmetric/Hermitian diagonal scaling used by POSVX. */
enum class LapackCholeskyEquilibration : std::uint8_t {
  kNone,     ///< Unscaled matrix; upstream EQUED=N.
  kDiagonal  ///< A is diag(S)*original_A*diag(S); upstream EQUED=Y.
};

/** @brief Queries single real POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Posvx(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const float> original,
      DenseBlasMatrixView<float> factors, DenseBlasMatrixView<const float> rhs,
      DenseBlasMatrixView<float> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single real POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> original, DenseBlasMatrixView<float> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single real POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Posvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> original, DenseBlasMatrixView<double> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single complex POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Posvx(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const std::complex<float>> original,
      DenseBlasMatrixView<std::complex<float>> factors,
      DenseBlasMatrixView<const std::complex<float>> rhs,
      DenseBlasMatrixView<std::complex<float>> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single complex POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double complex POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex POSVX FACT=N.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Posvx(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<const std::complex<double>> original,
      DenseBlasMatrixView<std::complex<double>> factors,
      DenseBlasMatrixView<const std::complex<double>> rhs,
      DenseBlasMatrixView<std::complex<double>> solution,
      DenseBlasVectorView<double> forward_error,
      DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPosvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex POSVX FACT=E.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate selected Cholesky factor output; old entries are
 * unread.
 * @param equilibration Actual scaling output; old enum value is unread.
 * @param scales Contiguous n-entry underlying-real scale output.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double complex POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex POSVX FACT=F.
 * @param provider Explicit checked reference selection; no fallback.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable square A; already scaled for supplied-factor mode.
 * @param factors Immutable matching selected Cholesky factor AF.
 * @param equilibration Actual scaling already applied to A and its supplied
 * factor.
 * @param scales Contiguous supplied S; only selected scales are read.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint scalar/real/integer/packing storage.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or provider
 * defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PosvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_DRIVER_H_
