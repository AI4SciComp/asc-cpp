#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_DRIVER_H_
/** @file
 * @brief Checked single-stage Aasen factor-and-solve drivers.
 *
 * Selected square A is an original symmetric or Hermitian matrix. SY uses
 * transpose symmetry, including complex symmetric inputs; HE uses adjoints
 * and ignores original imaginary diagonal components. On successful execution
 * selected A contains tridiagonal T and shifted unit triangular multipliers,
 * and contiguous output pivots contain positive one-based Aasen interchanges.
 * These outputs may be reused together with the matching SYTRS_AA/HETRS_AA
 * interface and kAasen raw pivot provenance. Padding and unselected A remain
 * unchanged; original A and output factors have different storage semantics.
 *
 * Queries inspect metadata only. For n>0, caller scalar WORK has minimum
 * max(2*n,3*n-2), rounded preferred capacity based on the pinned producer and
 * solve queries, n provider INTEGER objects in caller byte storage, and live
 * scalar packing storage for row A, all original HE A, and row B. Original and
 * effective strides, source integer expressions, byte counts, placement,
 * aliases and plan identity are checked before mutation. No allocation or
 * floating-point environment change occurs. Execution invokes the named native
 * driver, including that driver's internal workspace queries.
 *
 * Empty n is a validated noncall with no array reads or scratch. With n>0,
 * zero RHS still factorizes A and produces pivots; B remains unchanged. The
 * Aasen producer may complete with a singular T, so INFO zero is not an
 * invertibility or accuracy certificate. Incoming output pivots are unread.
 * Structural rejection preserves arrays and scratch; unsafe report/metadata
 * aliases also preserve the report. Otherwise the report resets at entry.
 *
 * Full-width INFO, complete valid output pivots and the final native WORK
 * recommendation are checked. Consistent positive GTSV INFO reports kNumerical,
 * kSingular and kDocumentedPartial: A/pivots contain reusable factor storage,
 * but B is not a usable solution. The native driver completes its final
 * triangular operations after GTSV failure. Packed B publication is withheld;
 * direct B may change. Missing or inconsistent provider outputs are defects:
 * packed A/B and public pivots are withheld, while direct A/B may have changed.
 * No provider correction, hidden scaling or alternate algorithm is selected.
 * Concurrent calls may share immutable provider/plans with distinct writable
 * A, pivots, B, workspace and reports.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Queries single real symmetric Aasen driver storage without array
 * reads.
 * @param provider Explicit Reference provider and selected integer ABI.
 * @param triangle Selected triangle of the original square matrix.
 * @param matrix Mutable original A; n>0 factorization still occurs with no RHS.
 * @param pivots Disjoint contiguous exact-n output; incoming values are unread.
 * @param rhs Disjoint n-by-nrhs mutable right-hand sides.
 * @return A provider/scalar/shape/layout/stride-bound caller workspace plan,
 * or a structural, placement, alias or representability error. Empty n needs
 * no scratch; active scalar minimum is max(2*n,3*n-2). Packing and native
 * INTEGER units are described by the plan and file contract.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<float> rhs);
/** @brief Executes the native single real symmetric Aasen driver.
 * @param provider Provider whose identity matches plan.
 * @param triangle Original matrix triangle bound into plan.
 * @param matrix Original A, overwritten with single-stage Aasen factors.
 * @param pivots Contiguous exact-n positive one-based interchange output.
 * @param rhs Original B, overwritten with X only on successful completion.
 * Direct B may change on failure; packed B is withheld as described above.
 * @param plan Matching checked query result; altered or stale plans reject.
 * @param workspace Disjoint caller regions with live scalar objects and native
 * INTEGER byte storage; n>0 scalar capacity is at least max(2*n,3*n-2).
 * @param report Per-call identity, raw INFO and output validity. Positive INFO
 * gives zero-based singular diagnostic and documented partial A/pivot output;
 * solution B is unusable. INFO zero certifies completion, not numerical
 * accuracy.
 * @return Success, pre-call structural failure, consistent singular numerical
 * failure, or provider defect. Empty n does not call the provider; zero RHS
 * with n>0 still produces factors and pivots. No hidden allocation or fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real symmetric Aasen driver storage without array
 * reads.
 * @param provider Explicit Reference provider and selected integer ABI.
 * @param triangle Selected triangle of the original square matrix.
 * @param matrix Mutable original A; n>0 factorization still occurs with no RHS.
 * @param pivots Disjoint contiguous exact-n output; incoming values are unread.
 * @param rhs Disjoint n-by-nrhs mutable right-hand sides.
 * @return A provider/scalar/shape/layout/stride-bound caller workspace plan,
 * or a structural, placement, alias or representability error. Empty n needs
 * no scratch; active scalar minimum is max(2*n,3*n-2). Packing and native
 * INTEGER units are described by the plan and file contract.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<double> rhs);
/** @brief Executes the native double real symmetric Aasen driver.
 * @param provider Provider whose identity matches plan.
 * @param triangle Original matrix triangle bound into plan.
 * @param matrix Original A, overwritten with single-stage Aasen factors.
 * @param pivots Contiguous exact-n positive one-based interchange output.
 * @param rhs Original B, overwritten with X only on successful completion.
 * Direct B may change on failure; packed B is withheld as described above.
 * @param plan Matching checked query result; altered or stale plans reject.
 * @param workspace Disjoint caller regions with live scalar objects and native
 * INTEGER byte storage; n>0 scalar capacity is at least max(2*n,3*n-2).
 * @param report Per-call identity, raw INFO and output validity. Positive INFO
 * gives zero-based singular diagnostic and documented partial A/pivot output;
 * solution B is unusable. INFO zero certifies completion, not numerical
 * accuracy.
 * @return Success, pre-call structural failure, consistent singular numerical
 * failure, or provider defect. Empty n does not call the provider; zero RHS
 * with n>0 still produces factors and pivots. No hidden allocation or fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex symmetric Aasen driver storage without array
 * reads.
 * @param provider Explicit Reference provider and selected integer ABI.
 * @param triangle Selected triangle of the original square matrix.
 * @param matrix Mutable original A; n>0 factorization still occurs with no RHS.
 * @param pivots Disjoint contiguous exact-n output; incoming values are unread.
 * @param rhs Disjoint n-by-nrhs mutable right-hand sides.
 * @return A provider/scalar/shape/layout/stride-bound caller workspace plan,
 * or a structural, placement, alias or representability error. Empty n needs
 * no scratch; active scalar minimum is max(2*n,3*n-2). Packing and native
 * INTEGER units are described by the plan and file contract.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes the native single complex symmetric Aasen driver.
 * @param provider Provider whose identity matches plan.
 * @param triangle Original matrix triangle bound into plan.
 * @param matrix Original A, overwritten with single-stage Aasen factors.
 * @param pivots Contiguous exact-n positive one-based interchange output.
 * @param rhs Original B, overwritten with X only on successful completion.
 * Direct B may change on failure; packed B is withheld as described above.
 * @param plan Matching checked query result; altered or stale plans reject.
 * @param workspace Disjoint caller regions with live scalar objects and native
 * INTEGER byte storage; n>0 scalar capacity is at least max(2*n,3*n-2).
 * @param report Per-call identity, raw INFO and output validity. Positive INFO
 * gives zero-based singular diagnostic and documented partial A/pivot output;
 * solution B is unusable. INFO zero certifies completion, not numerical
 * accuracy.
 * @return Success, pre-call structural failure, consistent singular numerical
 * failure, or provider defect. Empty n does not call the provider; zero RHS
 * with n>0 still produces factors and pivots. No hidden allocation or fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> matrix,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries double complex symmetric Aasen driver storage without array
 * reads.
 * @param provider Explicit Reference provider and selected integer ABI.
 * @param triangle Selected triangle of the original square matrix.
 * @param matrix Mutable original A; n>0 factorization still occurs with no RHS.
 * @param pivots Disjoint contiguous exact-n output; incoming values are unread.
 * @param rhs Disjoint n-by-nrhs mutable right-hand sides.
 * @return A provider/scalar/shape/layout/stride-bound caller workspace plan,
 * or a structural, placement, alias or representability error. Empty n needs
 * no scratch; active scalar minimum is max(2*n,3*n-2). Packing and native
 * INTEGER units are described by the plan and file contract.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes the native double complex symmetric Aasen driver.
 * @param provider Provider whose identity matches plan.
 * @param triangle Original matrix triangle bound into plan.
 * @param matrix Original A, overwritten with single-stage Aasen factors.
 * @param pivots Contiguous exact-n positive one-based interchange output.
 * @param rhs Original B, overwritten with X only on successful completion.
 * Direct B may change on failure; packed B is withheld as described above.
 * @param plan Matching checked query result; altered or stale plans reject.
 * @param workspace Disjoint caller regions with live scalar objects and native
 * INTEGER byte storage; n>0 scalar capacity is at least max(2*n,3*n-2).
 * @param report Per-call identity, raw INFO and output validity. Positive INFO
 * gives zero-based singular diagnostic and documented partial A/pivot output;
 * solution B is unusable. INFO zero certifies completion, not numerical
 * accuracy.
 * @return Success, pre-call structural failure, consistent singular numerical
 * failure, or provider defect. Empty n does not call the provider; zero RHS
 * with n>0 still produces factors and pivots. No hidden allocation or fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
SysvAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> matrix,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries single complex Hermitian Aasen driver storage without array
 * reads.
 * @param provider Explicit Reference provider and selected integer ABI.
 * @param triangle Selected triangle of the original square matrix.
 * @param matrix Mutable original A; n>0 factorization still occurs with no RHS.
 * @param pivots Disjoint contiguous exact-n output; incoming values are unread.
 * @param rhs Disjoint n-by-nrhs mutable right-hand sides.
 * @return A provider/scalar/shape/layout/stride-bound caller workspace plan,
 * or a structural, placement, alias or representability error. Empty n needs
 * no scratch; active scalar minimum is max(2*n,3*n-2). Packing and native
 * INTEGER units are described by the plan and file contract.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes the native single complex Hermitian Aasen driver.
 * @param provider Provider whose identity matches plan.
 * @param triangle Original matrix triangle bound into plan.
 * @param matrix Original A, overwritten with single-stage Aasen factors.
 * @param pivots Contiguous exact-n positive one-based interchange output.
 * @param rhs Original B, overwritten with X only on successful completion.
 * Direct B may change on failure; packed B is withheld as described above.
 * @param plan Matching checked query result; altered or stale plans reject.
 * @param workspace Disjoint caller regions with live scalar objects and native
 * INTEGER byte storage; n>0 scalar capacity is at least max(2*n,3*n-2).
 * @param report Per-call identity, raw INFO and output validity. Positive INFO
 * gives zero-based singular diagnostic and documented partial A/pivot output;
 * solution B is unusable. INFO zero certifies completion, not numerical
 * accuracy.
 * @return Success, pre-call structural failure, consistent singular numerical
 * failure, or provider defect. Empty n does not call the provider; zero RHS
 * with n>0 still produces factors and pivots. No hidden allocation or fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
HesvAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> matrix,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries double complex Hermitian Aasen driver storage without array
 * reads.
 * @param provider Explicit Reference provider and selected integer ABI.
 * @param triangle Selected triangle of the original square matrix.
 * @param matrix Mutable original A; n>0 factorization still occurs with no RHS.
 * @param pivots Disjoint contiguous exact-n output; incoming values are unread.
 * @param rhs Disjoint n-by-nrhs mutable right-hand sides.
 * @return A provider/scalar/shape/layout/stride-bound caller workspace plan,
 * or a structural, placement, alias or representability error. Empty n needs
 * no scratch; active scalar minimum is max(2*n,3*n-2). Packing and native
 * INTEGER units are described by the plan and file contract.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes the native double complex Hermitian Aasen driver.
 * @param provider Provider whose identity matches plan.
 * @param triangle Original matrix triangle bound into plan.
 * @param matrix Original A, overwritten with single-stage Aasen factors.
 * @param pivots Contiguous exact-n positive one-based interchange output.
 * @param rhs Original B, overwritten with X only on successful completion.
 * Direct B may change on failure; packed B is withheld as described above.
 * @param plan Matching checked query result; altered or stale plans reject.
 * @param workspace Disjoint caller regions with live scalar objects and native
 * INTEGER byte storage; n>0 scalar capacity is at least max(2*n,3*n-2).
 * @param report Per-call identity, raw INFO and output validity. Positive INFO
 * gives zero-based singular diagnostic and documented partial A/pivot output;
 * solution B is unusable. INFO zero certifies completion, not numerical
 * accuracy.
 * @return Success, pre-call structural failure, consistent singular numerical
 * failure, or provider defect. Empty n does not call the provider; zero RHS
 * with n>0 still produces factors and pivots. No hidden allocation or fallback.
 */
ASC_DENSE_LAPACK_EXPORT Status
HesvAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> matrix,
       DenseBlasVectorView<index_t> pivots,
       DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_DRIVER_H_
