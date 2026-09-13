#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_DRIVER_H_

/** @file
 * @brief Explicit reference general-tridiagonal simple and expert drivers.
 *
 * The lifetime, context, alias, report, allocation and plan contracts of
 * lapack_tridiagonal.h apply. No full matrix is formed and no global provider
 * selection occurs. All referenced arrays are disjoint contiguous CPU storage.
 * Queries read metadata and scalar options only; execution checks borrowed
 * adjacent pivot values after context admission. Raw factors and pivots must
 * originate together; validation cannot prove numerical provenance. No factor
 * report is fabricated, and report.factor_family remains absent.
 * GTSV overwrites DL/D/DU and B. It has no transpose mode. DL does not retain
 * GTTRF multipliers: these outputs must never be passed as reusable GTTRF
 * factors. Positive INFO publishes the actual partial elimination and B, not a
 * solution. For real n>0, nrhs=0 the pinned implementation still forms and
 * uses B(:,1) in its backsolve; the plan explicitly reserves n live kScratch
 * scalars initialized by ASC, with actual foreign NRHS unchanged at zero and
 * public empty B untouched. Complex zero-RHS calls need no surrogate.
 * GTSVX uses FACT=N; GtsvxFactored uses FACT=F. Both accept N/T/C and
 * independent B/X layouts. Original tridiagonal A and B remain immutable.
 * FACT=N outputs all four GTTRF arrays and adjacent pivots. FACT=F never
 * changes them and active execution rejects zero factor D before writes.
 * FERR/BERR have nrhs real entries, increment one. RCOND is a disjoint live
 * host real object. Positive FACT=N INFO<=n publishes completed raw GTTRF
 * factors/pivots and RCOND=0 but leaves X/FERR/BERR unchanged; these are not
 * successful factors. INFO=n+1 is an accuracy warning with computed solution
 * and error estimates, not a success certificate. Negative or impossible
 * INFO/diagnostics are provider defects; nonfinite diagnostics remain visible
 * with partial accuracy-warning validity. No blanket finite-input or
 * finite-solution guarantee is made. Expert kInteger contains n native pivots,
 * followed for real types by a distinct simultaneous n-integer estimator
 * array; complex types need only n. Expert scratch is the GTRFS fixed
 * capacity, plus one n*nrhs scalar region for each row-major B/X. Output-only X
 * is never read before entry. Empty n completes locally: RCOND=1 and
 * FERR/BERR=0, no foreign INFO. For n>0, nrhs=0 factorization/condition
 * estimation still occurs with valid private dummy B/X arguments, actual
 * NRHS=0, and no public RHS/solution element accesses. Neither driver
 * synthesizes a GTTRF report; raw matching expert factors can be used by
 * GTCON/GTRFS or FACT=F, not by the success-only GTTRF factory.
 * For the pinned singleton A=B=min_normal/8, both expert modes return raw
 * RCOND=0, FERR=Inf and INFO=n+1 despite the exact condition being one and
 * solution being representable. ASC retains these warning outputs. The
 * required extreme-scaling mathematical gate remains incomplete.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_tridiagonal.h"

namespace asc {

/** @brief Checks single real GTSV metadata and explicit staging.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @return A fixed-capacity matching plan, with no numerical reads or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<float> matrix, DenseBlasMatrixView<float> rhs);
/** @brief Solves a single real tridiagonal system by the actual GTSV driver.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @param plan Unmodified matching query result, including real zero-RHS
 * scratch.
 * @param workspace Disjoint caller staging; all scalar objects already live.
 * @param report Mandatory report retaining raw INFO and partial-output
 * diagnosis.
 * @return OK, structural failure, singular partial result or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtsv(const ReferenceLapackProvider& provider,
                                    LapackTridiagonalView<float> matrix,
                                    DenseBlasMatrixView<float> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Checks single real GTSVX FACT=N capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<float> factors,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, const float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes single real GTSVX with FACT=N.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<float> factors,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks single real GTSVX FACT=F capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, const float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes single real GTSVX with FACT=F.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const float> original,
    LapackTridiagonalLuStorage<const float> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const float> rhs,
    DenseBlasMatrixView<float> solution, float& reciprocal_condition,
    DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double real GTSV metadata and explicit staging.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @return A fixed-capacity matching plan, with no numerical reads or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<double> matrix, DenseBlasMatrixView<double> rhs);
/** @brief Solves a double real tridiagonal system by the actual GTSV driver.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @param plan Unmodified matching query result, including real zero-RHS
 * scratch.
 * @param workspace Disjoint caller staging; all scalar objects already live.
 * @param report Mandatory report retaining raw INFO and partial-output
 * diagnosis.
 * @return OK, structural failure, singular partial result or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtsv(const ReferenceLapackProvider& provider,
                                    LapackTridiagonalView<double> matrix,
                                    DenseBlasMatrixView<double> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Checks double real GTSVX FACT=N capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<double> factors,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes double real GTSVX with FACT=N.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<double> factors,
    DenseBlasVectorView<index_t> pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double real GTSVX FACT=F capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes double real GTSVX with FACT=F.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const double> original,
    LapackTridiagonalLuStorage<const double> factors,
    ReferenceTridiagonalPivotView pivots, DenseBlasMatrixView<const double> rhs,
    DenseBlasMatrixView<double> solution, double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks single complex GTSV metadata and explicit staging.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @return A fixed-capacity matching plan, with no numerical reads or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Solves a single complex tridiagonal system by the actual GTSV driver.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @param plan Unmodified matching query result, including real zero-RHS
 * scratch.
 * @param workspace Disjoint caller staging; all scalar objects already live.
 * @param report Mandatory report retaining raw INFO and partial-output
 * diagnosis.
 * @return OK, structural failure, singular partial result or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gtsv(const ReferenceLapackProvider& provider,
     LapackTridiagonalView<std::complex<float>> matrix,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Checks single complex GTSVX FACT=N capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes single complex GTSVX with FACT=N.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks single complex GTSVX FACT=F capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);
/** @brief Executes single complex GTSVX with FACT=F.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<float>> original,
    LapackTridiagonalLuStorage<const std::complex<float>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double complex GTSV metadata and explicit staging.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @return A fixed-capacity matching plan, with no numerical reads or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Solves a double complex tridiagonal system by the actual GTSV driver.
 * @param provider Explicit checked reference provider.
 * @param matrix Mutable separate contiguous DL/D/DU arrays.
 * @param rhs Mutable n-by-nrhs B, overwritten by X only on success.
 * @param plan Unmodified matching query result, including real zero-RHS
 * scratch.
 * @param workspace Disjoint caller staging; all scalar objects already live.
 * @param report Mandatory report retaining raw INFO and partial-output
 * diagnosis.
 * @return OK, structural failure, singular partial result or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gtsv(const ReferenceLapackProvider& provider,
     LapackTridiagonalView<std::complex<double>> matrix,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Checks double complex GTSVX FACT=N capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes double complex GTSVX with FACT=N.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Mutable output LU storage with separate DU2; initial values
 * ignored.
 * @param pivots Mutable exact-n ASC index_t output, increment one; initial
 * values ignored.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Gtsvx(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Checks double complex GTSVX FACT=F capacities.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @return A metadata-bound plan without factor, pivot or output-value reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGtsvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);
/** @brief Executes double complex GTSVX with FACT=F.
 * @param provider Explicit checked reference provider.
 * @param transpose N, T or conjugate-transpose operation.
 * @param original Immutable original separate DL/D/DU arrays.
 * @param factors Immutable same-matrix LU and DU2, unchanged.
 * @param pivots Immutable same-factor adjacent pivots.
 * @param rhs Immutable n-by-nrhs B in either layout.
 * @param solution Output-only n-by-nrhs X, independently chosen layout.
 * @param reciprocal_condition Live disjoint host real RCOND output; query
 * leaves it unchanged.
 * @param forward_error Live exact-nrhs real FERR output vector.
 * @param backward_error Live disjoint exact-nrhs real BERR output vector.
 * @param plan Unmodified matching fixed-capacity plan.
 * @param workspace Caller-owned scalar, real, packing and distinct integer
 * regions.
 * @param report Mandatory raw-INFO report; driver output is not a GTTRF
 * certificate.
 * @return OK, structural error, singular/accuracy warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status GtsvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackTridiagonalView<const std::complex<double>> original,
    LapackTridiagonalLuStorage<const std::complex<double>> factors,
    ReferenceTridiagonalPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_DRIVER_H_
