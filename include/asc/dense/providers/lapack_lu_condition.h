#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_CONDITION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_CONDITION_H_

/** @file
 * @brief Explicit checked reference reciprocal-condition estimation from LU.
 *
 * GECON estimates, rather than exactly computes, the reciprocal condition
 * number in the chosen norm. The caller supplies the original matrix norm and
 * raw square LU storage produced by GETRF. No pivot array is needed. Complex
 * matrix norms use Euclidean scalar modulus, not abs(real)+abs(imag).
 *
 * Queries evaluate checked formulas only: real WORK has 4*n scalar entries
 * plus n ABI-width kInteger entries; complex WORK has 2*n scalar entries plus
 * 2*n underlying-real kReal entries. Row-major also needs n*n live scalar
 * kLayoutConversion entries, into which the original LU is packed without
 * transposition of its mathematical entries. Column-major needs no packing.
 * Scalar/real/packing objects must be alive; the implementation starts trivial
 * foreign-integer lifetimes in the checked byte region. There is no foreign
 * LWORK query, allocation, implicit conversion buffer, transfer or fallback.
 *
 * Plans bind routine/scalar, shape, layout, leading dimension, norm flag and
 * provider identity, not numerical values or buffer addresses. Execution
 * narrows only the actual foreign leading dimension; the original row-major
 * source stride remains ASC-sized metadata bound in the plan options. It
 * checks plans, all simultaneously live storage and finite nonnegative ANORM
 * before numerical mutation. A and caller padding are unchanged on every
 * return. Workspaces and RCOND are disjoint from A and each other.
 *
 * Structural failures leave RCOND and workspaces unchanged, with absent raw
 * INFO and called_provider false. For n=0, RCOND becomes one without a foreign
 * call. On actual INFO=0, RCOND is a complete estimate; zero can mean singular,
 * underflowed or extremely ill-conditioned, and is not turned into a fabricated
 * pivot failure. INFO=1 reports failed estimation (NaN/Inf RCOND or zero
 * estimated inverse norm), kNumerical/kAccuracyWarning, and documented partial
 * RCOND; there is no meaningful diagnostic index. Negative or unexpected INFO
 * is a provider defect with unusable RCOND. Reports preserve exact raw INFO.
 * Nonfinite LU entries are not scanned or given a successful-finiteness
 * promise.
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

/** @brief Original-matrix norm used by the GECON estimator. */
enum class LapackConditionNorm : std::uint8_t {
  kOne,      ///< Maximum column sum of scalar moduli; upstream '1'/'O'.
  kInfinity  ///< Maximum row sum of scalar moduli; upstream 'I'.
};

/** @brief Computes exact single real GECON workspace capacities.
 * @param provider Explicit checked provider; this formula query does not call
 * it.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host output object, unchanged.
 * @return Metadata-bound checked plan or a structural validation error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const float> factors, float original_norm,
    const float& reciprocal_condition);
/** @brief Estimates the single real reciprocal condition number from LU.
 * @param provider Explicit checked reference selection with no fallback.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host real RCOND output.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Caller-owned disjoint typed and foreign-integer storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK for a complete estimate, structural error, numerical estimation
 * failure, or provider defect; see the file-level INFO and validity contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasMatrixView<const float> factors, float original_norm,
      float& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact double real GECON workspace capacities.
 * @param provider Explicit checked provider; this formula query does not call
 * it.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host output object, unchanged.
 * @return Metadata-bound checked plan or a structural validation error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const double> factors, double original_norm,
    const double& reciprocal_condition);
/** @brief Estimates the double real reciprocal condition number from LU.
 * @param provider Explicit checked reference selection with no fallback.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host real RCOND output.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Caller-owned disjoint typed and foreign-integer storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK for a complete estimate, structural error, numerical estimation
 * failure, or provider defect; see the file-level INFO and validity contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasMatrixView<const double> factors, double original_norm,
      double& reciprocal_condition, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact single complex GECON workspace capacities.
 * @param provider Explicit checked provider; this formula query does not call
 * it.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host output object, unchanged.
 * @return Metadata-bound checked plan or a structural validation error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const std::complex<float>> factors, float original_norm,
    const float& reciprocal_condition);
/** @brief Estimates the single complex reciprocal condition number from LU.
 * @param provider Explicit checked reference selection with no fallback.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host real RCOND output.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Caller-owned disjoint typed and foreign-integer storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK for a complete estimate, structural error, numerical estimation
 * failure, or provider defect; see the file-level INFO and validity contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Gecon(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const std::complex<float>> factors, float original_norm,
    float& reciprocal_condition, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact double complex GECON workspace capacities.
 * @param provider Explicit checked provider; this formula query does not call
 * it.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host output object, unchanged.
 * @return Metadata-bound checked plan or a structural validation error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeconWorkspace(
    const ReferenceLapackProvider& provider, LapackConditionNorm norm,
    DenseBlasMatrixView<const std::complex<double>> factors,
    double original_norm, const double& reciprocal_condition);
/** @brief Estimates the double complex reciprocal condition number from LU.
 * @param provider Explicit checked reference selection with no fallback.
 * @param norm One or infinity norm, matching original_norm.
 * @param factors Immutable square host/pinned-host raw LU in either layout.
 * @param original_norm Finite nonnegative norm of the original unfactored A.
 * @param reciprocal_condition Live disjoint host real RCOND output.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Caller-owned disjoint typed and foreign-integer storage.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK for a complete estimate, structural error, numerical estimation
 * failure, or provider defect; see the file-level INFO and validity contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gecon(const ReferenceLapackProvider& provider, LapackConditionNorm norm,
      DenseBlasMatrixView<const std::complex<double>> factors,
      double original_norm, double& reciprocal_condition,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_CONDITION_H_
