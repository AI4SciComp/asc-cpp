#ifndef ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_DRIVER_H_

/** @file
 * @brief Explicit Reference PTSV with real D and lower real/complex E.
 *
 * The driver overwrites contiguous D[N] and E[max(N-1,0)] with unit lower
 * bidiagonal LDL^H factors (transpose for real scalars). No native UPLO,
 * pivoting or workspace argument exists. The original descriptor always means
 * lower E. For the same Hermitian matrix upper factors use conjugated E.
 *
 * Queries inspect metadata only. Active B is staged in N*NRHS live scalar
 * objects in kLayoutConversion for either full layout. D, E, B, metadata and
 * workspace must be disjoint under the existing expert workspace rules.
 * Preflight preserves numeric and scratch storage; metadata aliases also
 * preserve the report. Otherwise the report is reset before preflight.
 *
 * N=0 completes locally without native INFO or numeric reads. N>0, NRHS=0
 * still factors D/E through the provider, with no active B or scratch values.
 * INFO=1..N means a nonpositive leading minor: D/E are partial and B remains
 * unchanged. INFO<N leaves incomplete factors; INFO=N completes factorization
 * with a nonpositive final D. Invalid or unwritten INFO is a provider defect;
 * D/E native writes remain, but staged B is not published. INFO=0 publishes
 * raw results, qualifying nonfinite factors/solutions as accuracy warnings.
 * There is no rescaling, diagnostic clamp, hidden fallback or allocation.
 *
 * On finite successful completion, returned factors can be borrowed through
 * ReferencePositiveDefiniteTridiagonalFactorView::FromRaw with kLower and
 * reused by Pttrs, or explicitly copied by the existing factor owner. The
 * PTTRF-report-bound Create factory does not accept a PTSV report; no prior
 * PTTRF execution certificate is manufactured. Caller retains factor origin.
 * Cost is O(N+N*NRHS); explicit temporary storage is O(N*NRHS).
 * Independent calls and immutable-plan reuse require separate D/E/B,
 * workspace, reports and provider contexts. Link ASC::dense_lapack.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {
/** @brief Plans SPTSV without numeric access or native entry.
 * @param provider Explicit accessible host provider.
 * @param matrix Real D and lower E; both are overwritten on native entry.
 * @param rhs N-by-NRHS input/output matrix in either padded full layout.
 * @return Fixed plan or shape/access/alias/count/byte failure. Identity binds
 * scalar, provider, N, NRHS, original leading dimension and RHS layout;
 * addresses and values may change when reusing an immutable plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<float> matrix,
    DenseBlasMatrixView<float> rhs);
/** @brief Executes SPTSV with explicit transactional solution staging.
 * @param provider Same provider as the workspace plan.
 * @param matrix Original matrix, replaced by documented lower D/E factors.
 * @param rhs Right-hand sides; receives X only after valid zero native INFO.
 * @param plan Matching metadata identity and explicit storage requirement.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and bad-pivot index.
 * @return Success for finite outputs, kNumerical for nonpositive pivots or
 * nonfinite publication, kProvider for invalid/unwritten INFO, or preflight
 * failure before any numeric access or mutation. B remains unchanged on
 * nonzero INFO; direct D/E writes cannot be rolled back.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ptsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteTridiagonalView<float> matrix,
     DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
     const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans DPTSV without numeric access or native entry.
 * @param provider Explicit accessible host provider.
 * @param matrix Real D and lower E; both are overwritten on native entry.
 * @param rhs N-by-NRHS input/output matrix in either padded full layout.
 * @return Fixed plan or shape/access/alias/count/byte failure. Identity binds
 * scalar, provider, N, NRHS, original leading dimension and RHS layout;
 * addresses and values may change when reusing an immutable plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<double> matrix,
    DenseBlasMatrixView<double> rhs);
/** @brief Executes DPTSV with explicit transactional solution staging.
 * @param provider Same provider as the workspace plan.
 * @param matrix Original matrix, replaced by documented lower D/E factors.
 * @param rhs Right-hand sides; receives X only after valid zero native INFO.
 * @param plan Matching metadata identity and explicit storage requirement.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and bad-pivot index.
 * @return Success for finite outputs, kNumerical for nonpositive pivots or
 * nonfinite publication, kProvider for invalid/unwritten INFO, or preflight
 * failure before any numeric access or mutation. B remains unchanged on
 * nonzero INFO; direct D/E writes cannot be rolled back.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ptsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteTridiagonalView<double> matrix,
     DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
     const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans CPTSV without numeric access or native entry.
 * @param provider Explicit accessible host provider.
 * @param matrix Real D and lower E; both are overwritten on native entry.
 * @param rhs N-by-NRHS input/output matrix in either padded full layout.
 * @return Fixed plan or shape/access/alias/count/byte failure. Identity binds
 * scalar, provider, N, NRHS, original leading dimension and RHS layout;
 * addresses and values may change when reusing an immutable plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes CPTSV with explicit transactional solution staging.
 * @param provider Same provider as the workspace plan.
 * @param matrix Original matrix, replaced by documented lower D/E factors.
 * @param rhs Right-hand sides; receives X only after valid zero native INFO.
 * @param plan Matching metadata identity and explicit storage requirement.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and bad-pivot index.
 * @return Success for finite outputs, kNumerical for nonpositive pivots or
 * nonfinite publication, kProvider for invalid/unwritten INFO, or preflight
 * failure before any numeric access or mutation. B remains unchanged on
 * nonzero INFO; direct D/E writes cannot be rolled back.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ptsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);
/** @brief Plans ZPTSV without numeric access or native entry.
 * @param provider Explicit accessible host provider.
 * @param matrix Real D and lower E; both are overwritten on native entry.
 * @param rhs N-by-NRHS input/output matrix in either padded full layout.
 * @return Fixed plan or shape/access/alias/count/byte failure. Identity binds
 * scalar, provider, N, NRHS, original leading dimension and RHS layout;
 * addresses and values may change when reusing an immutable plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPtsvWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes ZPTSV with explicit transactional solution staging.
 * @param provider Same provider as the workspace plan.
 * @param matrix Original matrix, replaced by documented lower D/E factors.
 * @param rhs Right-hand sides; receives X only after valid zero native INFO.
 * @param plan Matching metadata identity and explicit storage requirement.
 * @param workspace Disjoint live typed storage described in this file.
 * @param report Actual native entry/INFO, output validity and bad-pivot index.
 * @return Success for finite outputs, kNumerical for nonpositive pivots or
 * nonfinite publication, kProvider for invalid/unwritten INFO, or preflight
 * failure before any numeric access or mutation. B remains unchanged on
 * nonzero INFO; direct D/E writes cannot be rolled back.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ptsv(const ReferenceLapackProvider& provider,
     LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_DRIVER_H_
