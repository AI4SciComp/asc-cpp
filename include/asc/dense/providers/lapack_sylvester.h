#ifndef ASC_DENSE_PROVIDERS_LAPACK_SYLVESTER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_SYLVESTER_H_

/** @file
 * @brief Explicit reference solutions of scaled ordinary Sylvester equations.
 *
 * TRSYL solves op(A) X + sign X op(B) = scale C. A is m-by-m, B is n-by-n,
 * and C is m-by-n, overwritten by X. A/B must already be upper Schur forms;
 * no Schur reduction, precision change, or alternate driver is selected.
 * Real inputs may contain nonoverlapping canonical 2-by-2 diagonal blocks:
 * equal finite diagonals and finite nonzero offdiagonals of opposite signs.
 * Execution validates these blocks before any numerical writes. Complex A/B
 * are upper triangular. The implicit lower triangle (below the first real
 * subdiagonal) and all padding are ignored, including nonfinite sentinels.
 *
 * All calls are CPU-only and allocation-free with explicit caller storage.
 * Both A/B layouts require full square layout-conversion buffers because
 * the pinned routine's norm calculation reads full matrices. Only meaningful
 * entries are copied; implicit zeros are filled without reading ignored
 * storage. Column-major C is direct; row-major C uses an additional m*n
 * caller buffer. The region must contain live aligned numerical scalar
 * objects. No allocation, transfer or synchronization is implied.
 *
 * SCALE is an underlying-real output in [0,1], never implicitly divided out.
 * INFO=1 indicates perturbed coefficients: return kNumerical with
 * kAccuracyWarning/kDocumentedPartial, retaining X and scale, without
 * certifying the unperturbed equation. A zero scale or nonfinite result
 * similarly retains the raw result with an accuracy warning, not success.
 * Negative or impossible INFO, or an invalid scale, is a provider defect:
 * preserve raw INFO, withhold scale and packed C, and mark direct C unusable.
 * All preflight failures leave numerical outputs unchanged. Any call-metadata
 * alias rejection preserves the report; otherwise reports are initialized
 * before remaining validation. A/B, C, scale, workspace and call metadata
 * must be disjoint. Queries read no numerical values and have no foreign call.
 *
 * Execution rejects finite diagonal coefficient sums
 * op(A_ii) + sign*op(B_jj) whose real or imaginary component exceeds the
 * scalar range. This returns kNumerical/kNotRun/kUnchanged before packing,
 * with unchanged C, scale and workspace, called_provider=false and no INFO.
 * The pinned routine can otherwise overflow that coefficient and return an
 * incorrect finite X with INFO=0. No implicit rescaling or replacement
 * algorithm is used; these finite-large modes remain unsupported. This
 * admission check is not a bound on every subsequent intermediate operation.
 *
 * Empty execution sets scale to one without reading A/B/C or entering LAPACK.
 * Plans still bind dimensions, original layouts/strides, operations, sign and
 * the exact provider. Successful empty execution has no fabricated native
 * INFO. No success certifies well-conditioning; preserve original C for a
 * scaled-equation residual check.
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

/** @brief Sign of the right matrix term in an ordinary Sylvester equation. */
enum class LapackSylvesterSign : std::int8_t {
  kPlus = 1,   ///< Solve op(A) X + X op(B) = scale C.
  kMinus = -1  ///< Solve op(A) X - X op(B) = scale C.
};

/** @brief Computes exact caller packing capacities for reference TRSYL.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param operation_a Real N/T/C or complex N/C; complex T is invalid.
 * @param operation_b Real N/T/C or complex N/C; complex T is invalid.
 * @param sign Plus or minus right matrix term; other values are invalid.
 * @param a Borrowed m-by-m Schur A, never modified or read by the query.
 * @param b Borrowed n-by-n Schur B, never modified or read by the query.
 * @param c Borrowed mutable m-by-n C; the query leaves it unchanged.
 * @param report Initialized diagnostics; called_provider is false, INFO absent.
 * @return Bound layout-conversion capacities or structural/overflow failure.
 * This is a formula query; TRSYL has no LWORK query interface.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<const float> b,
    DenseBlasMatrixView<float> c, LapackReport& report);

/** @brief Solves the scaled ordinary Sylvester equation using exact TRSYL.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param operation_a Same legal operation on A as in the plan.
 * @param operation_b Same legal operation on B as in the plan.
 * @param sign Same plus/minus term as in the plan.
 * @param a Borrowed input Schur A; only meaningful entries are read.
 * @param b Borrowed input Schur B; only meaningful entries are read.
 * @param c Borrowed C overwritten by X; retain original C for residual checks.
 * @param scale Caller-owned underlying-real scale, published with usable X.
 * @param plan Unmodified matching formula-query result; stale plans fail.
 * @param workspace Explicit live scalar packing; all roles must be disjoint.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical warning, or validation/provider failure with the
 * publication and rollback semantics stated in this file's contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Trsyl(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const float> a, DenseBlasMatrixView<const float> b,
    DenseBlasMatrixView<float> c, float& scale, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact caller packing capacities for reference TRSYL.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param operation_a Real N/T/C or complex N/C; complex T is invalid.
 * @param operation_b Real N/T/C or complex N/C; complex T is invalid.
 * @param sign Plus or minus right matrix term; other values are invalid.
 * @param a Borrowed m-by-m Schur A, never modified or read by the query.
 * @param b Borrowed n-by-n Schur B, never modified or read by the query.
 * @param c Borrowed mutable m-by-n C; the query leaves it unchanged.
 * @param report Initialized diagnostics; called_provider is false, INFO absent.
 * @return Bound layout-conversion capacities or structural/overflow failure.
 * This is a formula query; TRSYL has no LWORK query interface.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const double> a, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<double> c, LapackReport& report);

/** @brief Solves the scaled ordinary Sylvester equation using exact TRSYL.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param operation_a Same legal operation on A as in the plan.
 * @param operation_b Same legal operation on B as in the plan.
 * @param sign Same plus/minus term as in the plan.
 * @param a Borrowed input Schur A; only meaningful entries are read.
 * @param b Borrowed input Schur B; only meaningful entries are read.
 * @param c Borrowed C overwritten by X; retain original C for residual checks.
 * @param scale Caller-owned underlying-real scale, published with usable X.
 * @param plan Unmodified matching formula-query result; stale plans fail.
 * @param workspace Explicit live scalar packing; all roles must be disjoint.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical warning, or validation/provider failure with the
 * publication and rollback semantics stated in this file's contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trsyl(const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
      DenseBlasTranspose operation_b, LapackSylvesterSign sign,
      DenseBlasMatrixView<const double> a, DenseBlasMatrixView<const double> b,
      DenseBlasMatrixView<double> c, double& scale,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact caller packing capacities for reference TRSYL.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param operation_a Real N/T/C or complex N/C; complex T is invalid.
 * @param operation_b Real N/T/C or complex N/C; complex T is invalid.
 * @param sign Plus or minus right matrix term; other values are invalid.
 * @param a Borrowed m-by-m Schur A, never modified or read by the query.
 * @param b Borrowed n-by-n Schur B, never modified or read by the query.
 * @param c Borrowed mutable m-by-n C; the query leaves it unchanged.
 * @param report Initialized diagnostics; called_provider is false, INFO absent.
 * @return Bound layout-conversion capacities or structural/overflow failure.
 * This is a formula query; TRSYL has no LWORK query interface.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<const std::complex<float>> b,
    DenseBlasMatrixView<std::complex<float>> c, LapackReport& report);

/** @brief Solves the scaled ordinary Sylvester equation using exact TRSYL.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param operation_a Same legal operation on A as in the plan.
 * @param operation_b Same legal operation on B as in the plan.
 * @param sign Same plus/minus term as in the plan.
 * @param a Borrowed input Schur A; only meaningful entries are read.
 * @param b Borrowed input Schur B; only meaningful entries are read.
 * @param c Borrowed C overwritten by X; retain original C for residual checks.
 * @param scale Caller-owned underlying-real scale, published with usable X.
 * @param plan Unmodified matching formula-query result; stale plans fail.
 * @param workspace Explicit live scalar packing; all roles must be disjoint.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical warning, or validation/provider failure with the
 * publication and rollback semantics stated in this file's contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trsyl(const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
      DenseBlasTranspose operation_b, LapackSylvesterSign sign,
      DenseBlasMatrixView<const std::complex<float>> a,
      DenseBlasMatrixView<const std::complex<float>> b,
      DenseBlasMatrixView<std::complex<float>> c, float& scale,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact caller packing capacities for reference TRSYL.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param operation_a Real N/T/C or complex N/C; complex T is invalid.
 * @param operation_b Real N/T/C or complex N/C; complex T is invalid.
 * @param sign Plus or minus right matrix term; other values are invalid.
 * @param a Borrowed m-by-m Schur A, never modified or read by the query.
 * @param b Borrowed n-by-n Schur B, never modified or read by the query.
 * @param c Borrowed mutable m-by-n C; the query leaves it unchanged.
 * @param report Initialized diagnostics; called_provider is false, INFO absent.
 * @return Bound layout-conversion capacities or structural/overflow failure.
 * This is a formula query; TRSYL has no LWORK query interface.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTrsylWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
    DenseBlasTranspose operation_b, LapackSylvesterSign sign,
    DenseBlasMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<std::complex<double>> c, LapackReport& report);

/** @brief Solves the scaled ordinary Sylvester equation using exact TRSYL.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param operation_a Same legal operation on A as in the plan.
 * @param operation_b Same legal operation on B as in the plan.
 * @param sign Same plus/minus term as in the plan.
 * @param a Borrowed input Schur A; only meaningful entries are read.
 * @param b Borrowed input Schur B; only meaningful entries are read.
 * @param c Borrowed C overwritten by X; retain original C for residual checks.
 * @param scale Caller-owned underlying-real scale, published with usable X.
 * @param plan Unmodified matching formula-query result; stale plans fail.
 * @param workspace Explicit live scalar packing; all roles must be disjoint.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical warning, or validation/provider failure with the
 * publication and rollback semantics stated in this file's contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Trsyl(const ReferenceLapackProvider& provider, DenseBlasTranspose operation_a,
      DenseBlasTranspose operation_b, LapackSylvesterSign sign,
      DenseBlasMatrixView<const std::complex<double>> a,
      DenseBlasMatrixView<const std::complex<double>> b,
      DenseBlasMatrixView<std::complex<double>> c, double& scale,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_SYLVESTER_H_
