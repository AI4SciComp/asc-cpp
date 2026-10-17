#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_ROBUST_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_ROBUST_H_

/** @file
 * @brief Experimental first-party robust packed positive-definite expert v1.
 *
 * Link availability requires ASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX=ON.
 * Installed CMake packages report ASCCpp_EXPERIMENTAL_ROBUST_PPSVX. The initial
 * admitted profile is the existing Linux x86_64 GNU 11.4 static provider facet.
 * Selecting these named APIs never enables an implicit fallback.
 *
 * Explicit RobustPpsvx, RobustPpsvxEquilibrated and RobustPpsvxFactored select
 * FACT N/E/F. They do not call or replace the pinned Reference implementation.
 * The provider supplies the existing serial context and plan/build identity;
 * report.called_provider stays false and native_info stays absent. The report
 * routine names identify asc_robust_[sdcz]ppsvx_v1, not a foreign routine.
 *
 * N preserves AP/B and returns separate AFP/X. E may form C=D*A*D and D*B;
 * its S and equilibration outputs describe D. F consumes AP=C, matching AFP
 * and supplied D, and accepts original B. X solves the original system.
 * FACT F does not refactorize. Upper U is treated as L=conjugate-transpose(U).
 * RCOND estimates the reciprocal condition of C using the rounded factor
 * operator M: 1/(norm(C)*norm(M^-1)), with Euclidean complex modulus.
 * FERR estimates original-coordinate
 * forward error and BERR is componentwise in scaled coordinates. Neither
 * estimate is a universal certificate for arbitrary caller-supplied factors.
 * A zero solution norm uses an absolute forward estimate. Small residual
 * denominators retain the (n+1)*normal_min safe-floor convention, evaluated
 * in scaled arithmetic; an exact zero component can therefore report BERR=1.
 *
 * All numeric inputs must be finite; AP imaginary diagonals are ignored.
 * Supplied factors use full complex values and selected S is finite positive.
 * N/E never read old AFP values; no mode reads old X/FERR/BERR/RCOND values.
 * Queries and invalid-workspace/stale-plan paths inspect metadata only.
 * All descriptors retain the existing independent layouts, bounds, disjoint
 * ownership, serial CPU accessibility and explicit caller workspace rules.
 *
 * A kScratch region is untyped, suitably aligned byte storage with the exact
 * query capacity. The implementation establishes private trivially destructible
 * mantissa/exponent object lifetimes there with nonallocating placement new.
 * No other region is required. The query counts 2*p+2*n*nrhs+5*n+2*nrhs
 * private entries, with entry size/alignment supplied by the plan. No private
 * type is part of the public API. No hidden allocation or dense conversion
 * occurs. Per-real-component mantissas retain the selected scalar precision;
 * integer exponents extend range without changing the floating-point state.
 * N/E factorization and condition estimation each take cubic work in n.
 * Every RHS forward estimate applies n weighted basis solves, also cubic;
 * total work is O(n^3*(1+nrhs)), including FACT F. Scratch is O(n^2+n*nrhs).
 * This v1 algorithm prioritizes explicit range handling, not low-cost
 * estimates.
 *
 * All numeric outputs are staged. Structural failure, nonfinite input,
 * nonpositive factorization and unrepresentable publication preserve all
 * caller numeric values. Nonpositive factorization reports its zero-based
 * pivot; supplied zero diagonals report singularity. Publication that would
 * become nonfinite/zero from a nonzero value, or lose more than eight epsilon
 * relatively through subnormal rounding, returns kOverflow and an accuracy
 * warning with unchanged outputs. This is an explicit range limitation, not a
 * clamped estimate. Finite results with RCOND below epsilon or an excessive
 * unfloored residual ratio publish documented partial outputs and return an
 * accuracy warning. Only complete success in N/E authorizes computed Cholesky
 * factor reuse.
 *
 * N=0 completes locally with RCOND=1, FERR/BERR=0, E equilibration none and
 * no input/scratch reads. N>0 with NRHS=0 still factors and estimates
 * condition. Calls may run concurrently with independent
 * buffers/workspaces/reports and valid independent contexts. No shared mutable
 * workspace or report is safe.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single real PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> original,
    DenseBlasPackedMatrixView<float> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> original,
    DenseBlasPackedMatrixView<float> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single real PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const float> original,
    DenseBlasPackedMatrixView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> original,
    DenseBlasPackedMatrixView<double> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> original,
    DenseBlasPackedMatrixView<double> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const double> original,
    DenseBlasPackedMatrixView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single complex PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
RobustPpsvx(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasPackedMatrixView<const std::complex<float>> original,
            DenseBlasPackedMatrixView<std::complex<float>> factors,
            DenseBlasMatrixView<const std::complex<float>> rhs,
            DenseBlasMatrixView<std::complex<float>> solution,
            DenseBlasVectorView<float> forward_error,
            DenseBlasVectorView<float> backward_error,
            float& reciprocal_condition, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> original,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single complex PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<float>> original,
    DenseBlasPackedMatrixView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double complex PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @return Metadata-bound formula plan or structural error; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryRobustPpsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex PPSVX FACT=N.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Separate ordinary packed AFP output; old entries are
 * unread.
 * @param rhs Immutable original n-by-nrhs right-hand sides.
 * @param solution Separate n-by-nrhs X output; old entries are unread.
 * @param forward_error Contiguous nrhs-entry real FERR output.
 * @param backward_error Contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Live disjoint real RCOND output; old value is
 * unread.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
RobustPpsvx(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
            DenseBlasPackedMatrixView<const std::complex<double>> original,
            DenseBlasPackedMatrixView<std::complex<double>> factors,
            DenseBlasMatrixView<const std::complex<double>> rhs,
            DenseBlasMatrixView<std::complex<double>> solution,
            DenseBlasVectorView<double> forward_error,
            DenseBlasVectorView<double> backward_error,
            double& reciprocal_condition, const LapackWorkspacePlan& plan,
            const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
QueryRobustPpsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex PPSVX FACT=E.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Original square A, possibly equilibrated in place.
 * @param factors Separate ordinary packed AFP output; old entries are
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxEquilibrated(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> original,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double complex PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryRobustPpsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex PPSVX FACT=F.
 * @param provider Explicit context/build identity; ASC robust v1 numerical
 * route.
 * @param triangle Stored upper/lower triangle of A and AF.
 * @param original Immutable ordinary packed AP; already scaled in
 * supplied-factor mode.
 * @param factors Immutable matching ordinary packed Cholesky factor AFP.
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
 * @param workspace Explicit disjoint aligned untyped byte scratch with query
 * capacity.
 * @param report Mandatory failure-surviving diagnostics; see metadata-alias
 * exception.
 * @return OK, structural/numerical failure, accuracy warning, or explicit
 * range/accuracy warning.
 */
ASC_DENSE_LAPACK_EXPORT Status RobustPpsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<const std::complex<double>> original,
    DenseBlasPackedMatrixView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_PACKED_ROBUST_H_
