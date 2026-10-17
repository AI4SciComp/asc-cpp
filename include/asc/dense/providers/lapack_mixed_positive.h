#ifndef ASC_DENSE_PROVIDERS_LAPACK_MIXED_POSITIVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_MIXED_POSITIVE_H_
/** @file
 * @brief Explicit pinned DSPOSV/ZCPOSV positive-definite mixed solves.
 *
 * The selected triangle defines symmetric/Hermitian A. Complex imaginary
 * diagonals are not inputs. A/B/X layouts are independent; B is immutable and
 * X always stages without reading old outputs. Real column-major A is direct;
 * real row-major and every complex A pack the selected triangle, normalizing
 * imaginary diagonals to zero. Query and rejected metadata/workspace calls
 * read no numeric operands. Workspace contains live typed kScalar/kLayout
 * double or complex-double, kScratch float or complex-float, and kReal double
 * objects for ZCPOSV. There are no pivots or native workspace queries.
 *
 * The provider attempts low-precision Cholesky and working refinement, then
 * its documented working-precision fallback. A remains unchanged on low
 * success; working fallback publishes only the selected Cholesky triangle.
 * An INFO>0 leading-minor failure publishes documented partial A, retaining
 * old complex imaginary diagonals after return, and withholds X. Invalid
 * INFO/ITER withholds packed A and X; direct A writes cannot be rolled back.
 * Nonfinite/unwritten computed X publishes NaN/nonfinite values with an
 * accuracy warning, without clamping. Reports and statistics retain native
 * diagnostics on non-OK outcomes; preflight preserves old statistics/numerics.
 *
 * Successful working factors are reusable through LapackCholeskyFactorView
 * and Potrs with the same provider and triangle. Low scratch factors do not
 * certify A. ITER has the same meanings as LapackMixedSolveStatistics; its
 * factor_scalar refers to Cholesky here. ITER is a stopping/fallback report,
 * not a condition or forward-error certificate. No RCOND/FERR/BERR is inferred.
 * This explicitly chosen native mixed driver changes no default or fallback.
 *
 * N=0 completes locally without numeric access or native INFO/ITER. N>0,
 * NRHS=0 still calls the provider for norm/factor work. The plan accounts for
 * native loop/index arithmetic and the inactive SX actual argument address.
 * Cost is O(n^3) plus O(n^2*nrhs) per refinement (at most 30), possible second
 * working factorization, and O(n^2+n*nrhs) explicit scratch. There is no ASC
 * operation allocation or transfer. Concurrent calls need independent mutable
 * operands, reports, statistics and workspace; immutable B/plans may be shared
 * with matching metadata. Existing provider/platform/ABI admission applies.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_mixed_general.h"

namespace asc {
/** @brief Queries checked explicit workspace for the actual real mixed driver.
 * @param provider Admitted pinned provider with actual integer ABI identity.
 * @param a Mutable square symmetric double A; entries are unread by this query.
 * @param uplo Selected symmetric/Hermitian triangle; the other is untouched.
 * @param b Immutable n-by-nrhs input RHS, unread by query.
 * @param x Output n-by-nrhs solution; old values never become inputs.
 * @return Metadata-bound plan or structural error, without allocation/entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryDsposvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> a,
    DenseBlasTriangle uplo, DenseBlasMatrixView<const double> b,
    DenseBlasMatrixView<double> x);
/** @brief Executes the explicit DSPOSV refinement/fallback contract above.
 * @param provider Same provider/build/ABI bound by the query.
 * @param a Original A, overwritten only by the native working fallback.
 * @param uplo Same selected triangle bound by the query.
 * @param b Immutable original RHS, disjoint from every mutable operand.
 * @param x Staged solution, published only on INFO=0 and valid ITER.
 * @param plan Unmodified matching metadata query result.
 * @param workspace Live typed scratch and output/norm staging above.
 * @param statistics Retained ITER, fallback reason and selected factor type.
 * @param report Retained exact INFO and qualified numerical outcome.
 * @return OK, preflight/provider failure, nonpositive leading minor or accuracy
 * warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Dsposv(const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> a,
       DenseBlasTriangle uplo, DenseBlasMatrixView<const double> b,
       DenseBlasMatrixView<double> x, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackMixedSolveStatistics& statistics,
       LapackReport& report);
/** @brief Queries checked workspace for the actual complex mixed driver.
 * @param provider Admitted pinned provider with actual integer ABI identity.
 * @param a Mutable square Hermitian complex-double A; query reads only
 * metadata.
 * @param uplo Selected symmetric/Hermitian triangle; the other is untouched.
 * @param b Immutable n-by-nrhs complex-double RHS, unread by query.
 * @param x Output n-by-nrhs solution, with no old-value input dependence.
 * @return Plan with complex-float lower workspace and double real norm work.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryZcposvWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> a, DenseBlasTriangle uplo,
    DenseBlasMatrixView<const std::complex<double>> b,
    DenseBlasMatrixView<std::complex<double>> x);
/** @brief Executes the explicit ZCPOSV refinement/fallback contract above.
 * @param provider Same provider/build/ABI bound by the query.
 * @param a Original A, overwritten only by the native working fallback.
 * @param uplo Same selected triangle bound by the query.
 * @param b Immutable original RHS, disjoint from every mutable operand.
 * @param x Staged solution, published only on INFO=0 and valid ITER.
 * @param plan Unmodified matching metadata query result.
 * @param workspace Live typed scratch and output/norm staging above.
 * @param statistics Retained ITER, fallback reason and selected factor type.
 * @param report Retained exact INFO and qualified numerical outcome.
 * @return OK, preflight/provider failure, nonpositive leading minor or accuracy
 * warning.
 */
ASC_DENSE_LAPACK_EXPORT Status
Zcposv(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<std::complex<double>> a, DenseBlasTriangle uplo,
       DenseBlasMatrixView<const std::complex<double>> b,
       DenseBlasMatrixView<std::complex<double>> x,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackMixedSolveStatistics& statistics, LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_MIXED_POSITIVE_H_
