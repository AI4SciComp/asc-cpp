#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_BAND_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_BAND_CONDITION_H_

/** @file
 * @brief Explicit pinned CPU triangular band reciprocal condition estimates.
 *
 * TBCON estimates 1/(norm(A)*norm(inv(A))) in the selected one/infinity
 * norm, computing the matrix norm internally. Complex LANTB uses complex
 * modulus. RCOND=0 is not a singular-pivot diagnosis. N=0 returns RCOND=1
 * locally, called_provider=false and absent INFO, without narrowing unused
 * native dimensions or using workspace. RCOND is a disjoint live underlying-
 * real host object, seeded with NaN only after successful preflight.
 * The selected triangle and bandwidth come from the checked band descriptor.
 * KD may exceed N. Unit diagonals, opposite triangles and band corners are
 * ignored; full N*LD backing remains required. A stays borrowed and immutable.
 * Row-major A uses N*(KD+1) live T objects in caller kLayoutConversion storage;
 * column-major A is direct. Packing stays banded. Queries read metadata only,
 * allocate nothing and make no foreign call. Plans bind routine/scalar, all
 * shapes, options, original layouts/strides, effective native dimensions and
 * the exact provider/build/integer ABI; stale plans fail before mutation.
 * All numerical spans, workspace regions and provider/plan/workspace/report
 * metadata must be disjoint. Metadata alias rejection preserves the report;
 * other validation resets it without changing numerical/workspace storage.
 * Accessible host/pinned-host storage and the explicit CPU provider are
 * required. Caller-owned objects remain alive throughout each call. Disjoint
 * calls may run concurrently; callers synchronize shared mutable storage.
 * No implicit precision change, transfer, fallback, synchronization or ASC
 * allocation occurs. Link the optional ASC::dense_lapack facet explicitly.
 *
 * Active calls use 3*N scalar and N private ABI-integer entries for real T,
 * or 2*N complex and N underlying-real entries for complex T, plus explicit
 * layout packing. Caller establishes scalar/real lifetimes; ASC starts the
 * private integer array lifetime in supplied aligned byte storage. Checked
 * source arithmetic covers 3*N even for complex T, KD and loop intermediates,
 * and actual native strides. Numeric execution uses bounded O(N*(KD+1))
 * triangular passes per estimator iteration; workspace query is constant time.
 * Workspace contents may change after entry. No positive INFO is valid.
 * Negative, missing, partially written or impossible INFO is a provider defect:
 * raw signed INFO survives and outputs are unusable. With INFO=0, negative
 * estimates are provider defects. Nonnegative nonfinite estimates return
 * kNumerical with kAccuracyWarning/kDocumentedPartial; other estimates report
 * success/complete. Finite estimates are not accuracy certificates. Provider
 * arithmetic may underflow/overflow; no blanket finiteness scan is made.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_lu_condition.h"

namespace asc {

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasDiagonal diagonal, LapackTriangularBandView<const float> a,
    const float& rcond);

/** @brief Executes exact pinned TBCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
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
Tbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasDiagonal diagonal, LapackTriangularBandView<const float> a,
      float& rcond, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasDiagonal diagonal, LapackTriangularBandView<const double> a,
    const double& rcond);

/** @brief Executes exact pinned TBCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
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
Tbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasDiagonal diagonal, LapackTriangularBandView<const double> a,
      double& rcond, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasDiagonal diagonal,
    LapackTriangularBandView<const std::complex<float>> a, const float& rcond);

/** @brief Executes exact pinned TBCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
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
Tbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasDiagonal diagonal,
      LapackTriangularBandView<const std::complex<float>> a, float& rcond,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
 * unread.
 * @param norm One or infinity norm; invalid enums are rejected.
 * @param rcond Disjoint live underlying-real host output; query does not read
 * it.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasDiagonal diagonal,
    LapackTriangularBandView<const std::complex<double>> a,
    const double& rcond);

/** @brief Executes exact pinned TBCON with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable checked triangular band matrix; ignored entries stay
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
Tbcon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasDiagonal diagonal,
      LapackTriangularBandView<const std::complex<double>> a, double& rcond,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_BAND_CONDITION_H_
