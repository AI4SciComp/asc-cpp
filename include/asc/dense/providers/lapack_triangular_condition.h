#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_CONDITION_H_

/** @file
 * @brief Explicit pinned CPU triangular reciprocal condition estimates.
 *
 * TRCON estimates 1/(norm(A)*norm(inv(A))) in the selected one/infinity norm,
 * computing the matrix norm internally and preserving A. Complex matrix norms
 * use complex modulus as in LANTR. RCOND=0 is not a singular-pivot diagnosis.
 * n=0 returns RCOND=1 locally, with called_provider=false and absent native
 * INFO. Row-major A needs n*n packing entries for an active call. RCOND is a
 * live underlying-real host object, seeded with NaN only after successful
 * preflight. Only the selected upper/lower triangle is read. The stored unit
 * diagonal, unused triangle and padding are ignored. Checked full square views
 * retain their complete reachable backing spans; all borrowed objects stay
 * alive. Layouts and strides are independent. Row-major inputs use explicit
 * live T objects in caller kLayoutConversion storage; column-major inputs are
 * direct. Queries inspect descriptors only, allocate nothing and never call
 * Fortran. Plans bind the routine, scalar, options, original layouts/strides,
 * effective foreign dimensions and exact provider/build/integer ABI. Stale
 * plans fail. All numerical spans, workspace regions and
 * provider/plan/workspace/report metadata must be disjoint. Detected metadata
 * aliases preserve the report; otherwise it is reset before preflight.
 * Structural rejection changes no numerical values or workspace. Accessible
 * host/pinned-host storage and the explicit optional CPU provider are required.
 * No implicit precision change, transfer, fallback, synchronization or
 * allocation occurs in ASC.
 *
 * Active calls require 3*n scalar and n private ABI-integer entries for real T,
 * or 2*n complex and n underlying-real entries for complex T. All other roles
 * except explicit packing are empty. Caller establishes scalar/real lifetimes;
 * ASC starts the private integer lifetimes in supplied aligned byte storage.
 * Source bounds include 3*n in LACN2 even for complex T, loop terminals and
 * actual foreign leading dimensions. Workspace values may change after entry.
 * No positive INFO is valid. Negative, missing, partially written or impossible
 * INFO is a provider defect: raw signed INFO survives, outputs are unusable,
 * and the native minimum is never negated. With INFO=0, negative estimates are
 * provider defects; nonnegative nonfinite estimates return kNumerical with
 * kAccuracyWarning/kDocumentedPartial. Other estimates report success/complete.
 * A finite estimate is not a conditioning or accuracy certificate. Provider
 * arithmetic may underflow/overflow; no blanket input-finiteness scan is made.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_lu_condition.h"

namespace asc {

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const float> a, const float& rcond);

/** @brief Executes exact pinned TRCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
      DenseBlasMatrixView<const float> a, float& rcond,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const double> a, const double& rcond);

/** @brief Executes exact pinned TRCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
      DenseBlasMatrixView<const double> a, double& rcond,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const std::complex<float>> a, const float& rcond);

/** @brief Executes exact pinned TRCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
      DenseBlasMatrixView<const std::complex<float>> a, float& rcond,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
    DenseBlasMatrixView<const std::complex<double>> a, const double& rcond);

/** @brief Executes exact pinned TRCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasTriangle triangle, DenseBlasDiagonal diagonal,
      DenseBlasMatrixView<const std::complex<double>> a, double& rcond,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_CONDITION_H_
