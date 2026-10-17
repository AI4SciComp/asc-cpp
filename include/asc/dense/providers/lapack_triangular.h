#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_H_

/** @file
 * @brief Explicit pinned CPU triangular inverse and reusable solve operations.
 *
 * TRTRI and TRTI2 overwrite the selected triangle with its inverse. TRTRS
 * solves op(A)*X=B, preserving A and overwriting B with X. Only full checked
 * square Dense BLAS views are accepted: compact/band/packed storage is not
 * reinterpreted. Each view retains the complete reachable backing span,
 * including gaps and padding; owners and live scalar objects survive the call.
 * Upper/lower and unit/nonunit are independent options. The unused triangle,
 * stored unit diagonal and padding are never read or changed. Real and complex
 * solves accept N/T/C; real T and C have the same mathematical meaning.
 *
 * A and B layouts/leading dimensions are independent. Row-major inverse uses
 * n*n live T objects in caller kLayoutConversion storage. Solve needs n*n
 * for row-major A and n*nrhs for row-major B when n and nrhs are nonzero;
 * column-major operands are direct. Queries inspect descriptors, never values,
 * and make no provider call (none of these routines has an LWORK interface).
 * All other workspace roles require zero capacity. Supplied regions, numeric
 * operands and provider/plan/workspace/report metadata must be disjoint; the
 * conservative check includes reachable padding. Any detected metadata alias
 * preserves the report. Otherwise it is reset before remaining preflight.
 * Structural rejection leaves numeric operands and workspace unchanged.
 * Plans bind scalar, all options, original layouts/strides, actual foreign
 * dimensions and the exact provider/build/integer ABI. Stale plans fail.
 *
 * TRTRI/TRTRS report native positive INFO for an exact zero nonunit diagonal:
 * kNumerical/kSingular/kUnchanged, with the zero-based diagnostic index.
 * TRTI2 has no singularity check or positive-INFO contract: a zero nonunit
 * diagonal still enters TRTI2 and may produce nonfinite values with INFO=0.
 * Nonfinite arithmetic otherwise follows the pin; success is not a finite
 * result, invertibility or conditioning certificate. Preserve original inputs
 * for independent residual checks. No new factor-family provenance is implied.
 * Negative or impossible INFO is a provider defect: preserve raw INFO, withhold
 * packed output and mark direct output unusable. Native-minimum INFO never
 * undergoes overflowing negation. Native writes cannot be rolled back.
 *
 * n=0 succeeds locally without touching values or fabricating INFO. With n>0,
 * TRTRS still calls the provider when nrhs=0 and still checks a nonunit
 * diagonal. Source-derived bounds include native loop terminals and TRTRI block
 * cursors. Calls require accessible host/pinned-host storage and the explicit
 * optional reference provider. No allocation, precision conversion, transfer,
 * fallback, synchronization or global handler change occurs. Disjoint calls are
 * reentrant.
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

/** @brief Computes exact scalar packing capacities for TRTRI.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<float> a);

/** @brief Inverts the selected triangle through exact reference TRTRI.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trtri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasMatrixView<float> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTI2.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<float> a);

/** @brief Inverts the selected triangle through exact reference TRTI2.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trti2(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasMatrixView<float> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRS.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Unchanged n-by-nrhs RHS descriptor, independently laid out.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<float> b);

/** @brief Solves op(A)*X=B through exact reference TRTRS.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out; overwritten by X.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trtrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const float> a, DenseBlasMatrixView<float> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRI.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<double> a);

/** @brief Inverts the selected triangle through exact reference TRTRI.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trtri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasMatrixView<double> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTI2.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<double> a);

/** @brief Inverts the selected triangle through exact reference TRTI2.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trti2(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasMatrixView<double> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRS.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Unchanged n-by-nrhs RHS descriptor, independently laid out.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const double> a, DenseBlasMatrixView<double> b);

/** @brief Solves op(A)*X=B through exact reference TRTRS.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out; overwritten by X.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trtrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const double> a, DenseBlasMatrixView<double> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRI.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<float>> a);

/** @brief Inverts the selected triangle through exact reference TRTRI.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trtri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasMatrixView<std::complex<float>> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTI2.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<float>> a);

/** @brief Inverts the selected triangle through exact reference TRTI2.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trti2(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasMatrixView<std::complex<float>> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRS.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Unchanged n-by-nrhs RHS descriptor, independently laid out.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b);

/** @brief Solves op(A)*X=B through exact reference TRTRS.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out; overwritten by X.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status Trtrs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRI.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<double>> a);

/** @brief Inverts the selected triangle through exact reference TRTRI.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trtri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<double>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTI2.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrti2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<double>> a);

/** @brief Inverts the selected triangle through exact reference TRTI2.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trti2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasMatrixView<std::complex<double>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact scalar packing capacities for TRTRS.
 * @param provider Explicit pinned CPU provider, borrowed for the query.
 * @param triangle Upper or lower stored triangle; other values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; other values are
 * rejected.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Unchanged n-by-nrhs RHS descriptor, independently laid out.
 * @return Descriptor-bound formula plan or structural/overflow failure.
 * No numerical values, native routines or allocation are used by this query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b);

/** @brief Solves op(A)*X=B through exact reference TRTRS.
 * @param provider Same exact provider/build and ABI as the formula query.
 * @param triangle Same stored upper/lower triangle as in the plan.
 * @param diagonal Same explicit/unit diagonal option as in the plan.
 * @param operation N/T/C, including complex conjugate transpose.
 * @param a Borrowed square full matrix; ignored triangle/unit diagonal stay
 * untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out; overwritten by X.
 * @param plan Unmodified matching formula plan; stale plans fail preflight.
 * @param workspace Disjoint live scalar packing objects; see file contract.
 * @param report Mandatory surviving INFO, outcome and output-validity report.
 * @return OK or structural/numerical/provider failure with the publication
 * semantics stated in this file. Numerical nonfiniteness follows the pin.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trtrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasMatrixView<const std::complex<double>> a,
      DenseBlasMatrixView<std::complex<double>> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_H_
