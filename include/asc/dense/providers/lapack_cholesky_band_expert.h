#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_EXPERT_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_EXPERT_H_

/** @file
 * @brief Explicit reference band expert solves with three factor modes.
 *
 * Pbsvx selects FACT=N, preserving original A/B and writing separate AF/X.
 * PbsvxEquilibrated selects FACT=E, computing S and possibly replacing A by
 * diag(S)*A*diag(S) and B by diag(S)*B. The returned equilibration is the
 * source's actual choice, not a command to force scaling. PbsvxFactored
 * selects FACT=F: A is already scaled as specified, AF factors that A, and
 * used supplied S is finite positive. A/AF/S are preserved; B may be scaled.
 * Source-completed X targets the original unscaled system in every mode;
 * the numerical limitations below can prevent a valid solution.
 *
 * Matching A/AF band descriptors carry order, bandwidth and triangle; their
 * layouts and the B/X layouts are independent. Only selected A entries and
 * real components of complex A diagonals are input. Raw FACT=F AF retains all
 * complex components; no diagonal predicate or factor certificate is added.
 * Unused supplied scales may have length zero or n and are never read.
 * Queries inspect metadata only; execution validates used scale values.
 *
 * Nonempty systems require real WORK=3*n scalars plus n private ABI-width
 * kInteger entries, or complex WORK=2*n scalars plus n underlying-real kReal
 * entries. Empty nrhs still factors/estimates condition. All four source
 * LACN2 routes require checked INTEGER 3*n. Query plans bind the exact source
 * dimensions, mode, layouts, original ASC strides, scale metadata and provider.
 *
 * Each row A/AF uses n*(kd+1) live kLayoutConversion scalars; each row B/X
 * uses n*nrhs. These regions are concatenated A, AF, B, X. Complex N/E A
 * always uses that band packing, including column-major: native COPY would
 * otherwise read ignored imaginary diagonals. Only selected components are
 * copied, with zero imaginary diagonals. Output-only AF/X packing never
 * reads prior outputs. There is no dense expansion, hidden allocation,
 * transfer, synchronization, fallback or handler mutation.
 *
 * An actual n=0 source call needs no work/input reads and writes RCOND=1,
 * FERR=BERR=0. This differs from the older POSVX local empty convention.
 * Caller scalar/real/packing objects are live; private integer lifetimes begin
 * in explicitly aligned caller bytes. All operands, outputs and nonempty
 * supplied workspace roles, including unused roles, pass the explicit
 * provider context. Existing zero-byte unused compatibility is retained.
 * Metadata aliases preserve an unsafe-to-reset report; other structural
 * failures reset it, preserve all numerical storage and avoid foreign entry.
 *
 * INFO in [1,n] in N/E is a factorization failure: applied A/B scaling,
 * partial S, selected AF and RCOND=0 survive; X/FERR/BERR are unchanged.
 * Native COPY initializes every selected AF entry before factorization.
 * Therefore partial row AF publication includes every selected component,
 * including the zero imaginary diagonals at future pivots. Old AF is unread;
 * this stronger source-defined result differs from POSVX's weaker partial
 * imaginary-output guarantee and from in-place PBTRF's prefix rule.
 * A is published only in E with actual EQUED=Y; otherwise ignored original
 * imaginary components stay untouched. Padding/corners are never published.
 *
 * INFO=n+1 retains source-completed X and estimates with an accuracy warning,
 * not an
 * invented exact singularity. RCOND describes the equilibrated matrix.
 * Nonfinite diagnostics produce a numerical accuracy warning; negative
 * diagnostics are provider-invalid. Raw INFO and outputs remain available
 * with documented-partial validity. Neither finite estimates nor INFO=0
 * certify exact bounds or finite X for arbitrary inputs. The pinned lower
 * complex scaled PBCON estimate and tiny PBRFS finite-estimate limitations
 * also affect this driver. Tiny FACT=E can overflow the source S*S*A
 * intermediate despite finite S and a representable exact scaled matrix;
 * no reassociation is performed. Faithful reporting is not mathematical
 * completion. Borrowed operands and caller storage must outlive each call.
 * Concurrent calls require disjoint mutable storage and reports. This is
 * CPU-only synchronous execution; checked queries do not call the provider.
 * Negative/impossible INFO or invalid native EQUED is a provider defect.
 * Packed outputs are not published for those invalid results; direct native
 * outputs cannot be rolled back. No route certifies supplied raw factors.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_cholesky_driver.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<float> factors,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsvx(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const float> original,
      LapackPositiveDefiniteBandView<float> factors,
      DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single real PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> original,
    LapackPositiveDefiniteBandView<float> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PbsvxEquilibrated(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<float> original,
    LapackPositiveDefiniteBandView<float> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single real PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single real PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PbsvxFactored(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> original,
    LapackPositiveDefiniteBandView<const float> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales, DenseBlasMatrixView<float> rhs,
    DenseBlasMatrixView<float> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Pbsvx(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<double> factors,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> original,
    LapackPositiveDefiniteBandView<double> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PbsvxEquilibrated(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<double> original,
    LapackPositiveDefiniteBandView<double> factors,
    LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double real PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double real PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PbsvxFactored(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> original,
    LapackPositiveDefiniteBandView<const double> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales, DenseBlasMatrixView<double> rhs,
    DenseBlasMatrixView<double> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single complex PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<std::complex<float>> factors,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsvx(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<float>> original,
      LapackPositiveDefiniteBandView<std::complex<float>> factors,
      DenseBlasMatrixView<const std::complex<float>> rhs,
      DenseBlasMatrixView<std::complex<float>> solution,
      DenseBlasVectorView<float> forward_error,
      DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<float>> original,
    LapackPositiveDefiniteBandView<std::complex<float>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
PbsvxEquilibrated(const ReferenceLapackProvider& provider,
                  LapackPositiveDefiniteBandView<std::complex<float>> original,
                  LapackPositiveDefiniteBandView<std::complex<float>> factors,
                  LapackCholeskyEquilibration& equilibration,
                  DenseBlasVectorView<float> scales,
                  DenseBlasMatrixView<std::complex<float>> rhs,
                  DenseBlasMatrixView<std::complex<float>> solution,
                  DenseBlasVectorView<float> forward_error,
                  DenseBlasVectorView<float> backward_error,
                  float& reciprocal_condition, const LapackWorkspacePlan& plan,
                  const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error,
    const float& reciprocal_condition);

/** @brief Executes single complex PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PbsvxFactored(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> original,
    LapackPositiveDefiniteBandView<const std::complex<float>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const float> scales,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, float& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries double complex PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<std::complex<double>> factors,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex PBSVX FACT=N.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected original, unscaled A.
 * @param factors Separate selected AF output; old entries are unread.
 * @param rhs Immutable original n-by-nrhs B.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbsvx(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<double>> original,
      LapackPositiveDefiniteBandView<std::complex<double>> factors,
      DenseBlasMatrixView<const std::complex<double>> rhs,
      DenseBlasMatrixView<std::complex<double>> solution,
      DenseBlasVectorView<double> forward_error,
      DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryPbsvxEquilibratedWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<std::complex<double>> original,
    LapackPositiveDefiniteBandView<std::complex<double>> factors,
    const LapackCholeskyEquilibration& equilibration,
    DenseBlasVectorView<double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex PBSVX FACT=E.
 * @param provider Explicit checked CPU reference provider.
 * @param original Original selected A, possibly scaled in place; complex
 * diagonal real-only.
 * @param factors Separate selected AF output; old entries are unread.
 * @param equilibration Actual source scaling output; old enum value is unread.
 * @param scales Contiguous n-entry real scale output; partial on source
 * failure.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
PbsvxEquilibrated(const ReferenceLapackProvider& provider,
                  LapackPositiveDefiniteBandView<std::complex<double>> original,
                  LapackPositiveDefiniteBandView<std::complex<double>> factors,
                  LapackCholeskyEquilibration& equilibration,
                  DenseBlasVectorView<double> scales,
                  DenseBlasMatrixView<std::complex<double>> rhs,
                  DenseBlasMatrixView<std::complex<double>> solution,
                  DenseBlasVectorView<double> forward_error,
                  DenseBlasVectorView<double> backward_error,
                  double& reciprocal_condition, const LapackWorkspacePlan& plan,
                  const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @return Checked formula plan or structural failure; no provider call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error,
    const double& reciprocal_condition);

/** @brief Executes double complex PBSVX FACT=F.
 * @param provider Explicit checked CPU reference provider.
 * @param original Immutable selected A, already scaled as equilibration
 * specifies.
 * @param factors Immutable matching raw band Cholesky AF; every complex
 * component retained.
 * @param equilibration Supplied scaling already applied to A and AF.
 * @param scales Contiguous supplied S; used values finite positive, unused
 * values unread.
 * @param rhs Original n-by-nrhs B, possibly scaled in place.
 * @param solution Separate n-by-nrhs solution output; old entries are unread.
 * @param forward_error Disjoint contiguous nrhs-entry real FERR output.
 * @param backward_error Disjoint contiguous nrhs-entry real BERR output.
 * @param reciprocal_condition Disjoint real RCOND output; old scalar value is
 * unread.
 * @param plan Unmodified matching checked formula plan.
 * @param workspace Explicit disjoint scalar/real/integer/band-layout storage.
 * @param report Mandatory failure-surviving raw INFO and validity report.
 * @return OK, preflight/numerical failure, accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status PbsvxFactored(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> original,
    LapackPositiveDefiniteBandView<const std::complex<double>> factors,
    LapackCholeskyEquilibration equilibration,
    DenseBlasVectorView<const double> scales,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, double& reciprocal_condition,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_EXPERT_H_
