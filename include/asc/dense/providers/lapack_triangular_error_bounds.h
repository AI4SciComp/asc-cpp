#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_ERROR_BOUNDS_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_ERROR_BOUNDS_H_

/** @file
 * @brief Explicit pinned CPU error estimates for a triangular solution.
 *
 * TRRFS preserves A, B and the supplied solution X of op(A)*X=B. It computes
 * FERR (estimated relative forward error) and BERR (componentwise relative
 * backward error); it performs no iterative refinement. N/T/C are distinct
 * complex options. Complex BERR uses abs(real)+abs(imag), including the pinned
 * small-denominator safeguard. FERR is an estimate, not a certified bound.
 * FERR/BERR are disjoint contiguous underlying-real views of exactly nrhs
 * elements. n=0 or nrhs=0 sets their elements to zero locally, without a
 * foreign call or fabricated INFO; inactive operands need no foreign integer
 * narrowing. Active row-major A/B/X need n*n/n*nrhs/n*nrhs packing entries
 * respectively. Output estimates are seeded with NaN only after successful
 * preflight. Inputs are never published or overwritten. No singularity scan is
 * introduced. Estimate-quality failures identify the first affected zero-based
 * RHS; negative outputs take precedence over nonfinite-output warnings. Only
 * the selected upper/lower triangle is read. The stored unit diagonal, unused
 * triangle and padding are ignored. Checked full square views retain their
 * complete reachable backing spans; all borrowed objects stay alive. Layouts
 * and strides are independent. Row-major inputs use explicit live T objects in
 * caller kLayoutConversion storage; column-major inputs are direct. Queries
 * inspect descriptors only, allocate nothing and never call Fortran. Plans bind
 * the routine, scalar, options, original layouts/strides, effective foreign
 * dimensions and exact provider/build/integer ABI. Stale plans fail. All
 * numerical spans, workspace regions and provider/plan/workspace/report
 * metadata must be disjoint. Detected metadata aliases preserve the report;
 * otherwise it is reset before preflight. Structural rejection changes no
 * numerical values or workspace. Accessible host/pinned-host storage and the
 * explicit optional CPU provider are required. No implicit precision change,
 * transfer, fallback, synchronization or allocation occurs in ASC.
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

namespace asc {

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<const float> b,
    DenseBlasMatrixView<const float> x, DenseBlasVectorView<float> ferr,
    DenseBlasVectorView<float> berr);

/** @brief Executes exact pinned TRRFS with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trrfs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const float> a, DenseBlasMatrixView<const float> b,
      DenseBlasMatrixView<const float> x, DenseBlasVectorView<float> ferr,
      DenseBlasVectorView<float> berr, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const double> a, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<const double> x, DenseBlasVectorView<double> ferr,
    DenseBlasVectorView<double> berr);

/** @brief Executes exact pinned TRRFS with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trrfs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const double> a, DenseBlasMatrixView<const double> b,
      DenseBlasMatrixView<const double> x, DenseBlasVectorView<double> ferr,
      DenseBlasVectorView<double> berr, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<const std::complex<float>> b,
    DenseBlasMatrixView<const std::complex<float>> x,
    DenseBlasVectorView<float> ferr, DenseBlasVectorView<float> berr);

/** @brief Executes exact pinned TRRFS with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trrfs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const std::complex<float>> a,
      DenseBlasMatrixView<const std::complex<float>> b,
      DenseBlasMatrixView<const std::complex<float>> x,
      DenseBlasVectorView<float> ferr, DenseBlasVectorView<float> berr,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact formula workspace capacities without reading values.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @return Matching plan or structural/overflow failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrrfsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<const std::complex<double>> x,
    DenseBlasVectorView<double> ferr, DenseBlasVectorView<double> berr);

/** @brief Executes exact pinned TRRFS with the file-level report contract.
 * @param provider Explicit pinned CPU provider borrowed throughout the call.
 * @param triangle Selected upper/lower triangle; invalid enums are rejected.
 * @param diagonal Explicit nonunit or implicit unit diagonal.
 * @param a Immutable square full triangular matrix; ignored entries stay
 * unread.
 * @param operation N/T/C equation option; invalid enums are rejected.
 * @param b Immutable RHS matrix with n rows and nrhs columns.
 * @param x Immutable solution with the same shape as B and independent layout.
 * @param ferr Disjoint contiguous real forward-error output of length nrhs.
 * @param berr Disjoint contiguous real backward-error output of length nrhs.
 * @param plan Unmodified formula plan for these descriptors/options/provider.
 * @param workspace Caller-owned disjoint live regions with the queried
 * capacity.
 * @param report Mandatory surviving raw INFO, outcome and validity report.
 * @return Success or structural/numerical/provider failure as documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trrfs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const std::complex<double>> a,
      DenseBlasMatrixView<const std::complex<double>> b,
      DenseBlasMatrixView<const std::complex<double>> x,
      DenseBlasVectorView<double> ferr, DenseBlasVectorView<double> berr,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_ERROR_BOUNDS_H_
