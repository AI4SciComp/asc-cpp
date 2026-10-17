#ifndef ASC_DENSE_PROVIDERS_LAPACK_LEAST_SQUARES_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LEAST_SQUARES_H_

/** @file
 * @brief Exact full-rank reference least-squares drivers with explicit storage.
 *
 * GELS uses QR/LQ, GELST uses compact-WY QR/LQ, and GETSLS uses the pinned
 * GEQR/GELQ algorithm. No driver substitutes for another or determines rank.
 * Full rank is a caller assumption; exact-zero triangular pivots alone are
 * diagnosed. Even INFO=0 for an all-zero input is the upstream convention,
 * not a full-rank certificate. Near rank deficiency can produce inaccurate
 * solutions without an error.
 *
 * A has m rows and n columns. B must have exactly max(m,n) rows and nrhs
 * columns, including output capacity beyond its input rows. N reads m rows
 * of B and returns n solution rows; real T or complex C reads n rows and
 * returns m solution rows. Tall effective systems minimize the Euclidean
 * residual; wide effective systems return the minimum Euclidean norm solution.
 *
 * All calls are CPU-only and reentrant with disjoint live operands, reports,
 * plans, workspaces and provider objects. No allocation, implicit transfer,
 * synchronization, precision change or provider selection occurs. The serial
 * provider admits host storage only, including every workspace region.
 * Queries use actual caller addresses but read no numerical values or padding.
 *
 * Execution packs row-major A and every B into caller layout-conversion
 * storage; B packing reads only the input rows. The region must contain live
 * aligned scalar objects. On success GELS/GELST publish all max(m,n) B rows,
 * including transformed residual coordinates in overdetermined cases.
 * Residual coordinates are not residual vectors; upstream scaling can leave
 * them scaled, so preserve original A/B to check residuals independently.
 * GETSLS publishes only solution rows; its undocumented trailing B rows are
 * unchanged. All drivers publish transformed A, not a reusable certified
 * factor: auxiliary reflectors can reside in overwritten WORK.
 *
 * Structural errors precede numerical writes and foreign calls. For valid
 * positive INFO, A and input-row B intermediates are retained, but no solution
 * is valid; scaling might not have been undone. The report is kSingular with
 * kDocumentedPartial and a zero-based failed triangular diagonal. Negative or
 * impossible INFO indicates a provider defect: packed outputs are withheld,
 * direct column-major A may be unusable, and the exact raw INFO survives.
 * Report overlap is rejected without resetting aliased storage. Otherwise
 * every call initializes the mandatory report before validation.
 *
 * Empty execution zeroes the documented B result rows without entering
 * LAPACK; empty queries remain real queries. Reports distinguish these cases
 * with called_provider and absent versus returned native_info. Neither
 * success nor failure implies finite outputs or a numerical rank certificate.
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

/** @brief Queries the exact QR/LQ GELS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
    LapackReport& report);

/** @brief Executes exact QR/LQ GELS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status Gels(const ReferenceLapackProvider& provider,
                                    DenseBlasTranspose transpose,
                                    DenseBlasMatrixView<float> matrix,
                                    DenseBlasMatrixView<float> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Queries the exact QR/LQ GELS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
    LapackReport& report);

/** @brief Executes exact QR/LQ GELS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status Gels(const ReferenceLapackProvider& provider,
                                    DenseBlasTranspose transpose,
                                    DenseBlasMatrixView<double> matrix,
                                    DenseBlasMatrixView<double> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Queries the exact QR/LQ GELS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report);

/** @brief Executes exact QR/LQ GELS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gels(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
     DenseBlasMatrixView<std::complex<float>> matrix,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries the exact QR/LQ GELS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report);

/** @brief Executes exact QR/LQ GELS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gels(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
     DenseBlasMatrixView<std::complex<double>> matrix,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries the exact compact-WY QR/LQ GELST workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
    LapackReport& report);

/** @brief Executes exact compact-WY QR/LQ GELST with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status Gelst(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose transpose,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasMatrixView<float> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries the exact compact-WY QR/LQ GELST workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
    LapackReport& report);

/** @brief Executes exact compact-WY QR/LQ GELST with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status Gelst(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose transpose,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasMatrixView<double> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries the exact compact-WY QR/LQ GELST workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report);

/** @brief Executes exact compact-WY QR/LQ GELST with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelst(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact compact-WY QR/LQ GELST workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls actual LWORK=-1 after source-pinned
 * minimum preflight. No numerical values, allocations or transfers are required
 * by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelstWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report);

/** @brief Executes exact compact-WY QR/LQ GELST with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelst(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact GEQR/GELQ GETSLS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls both actual LWORK=-2 and -1 queries.
 * No numerical values, allocations or transfers are required by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs,
    LapackReport& report);

/** @brief Executes exact GEQR/GELQ GETSLS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status Getsls(const ReferenceLapackProvider& provider,
                                      DenseBlasTranspose transpose,
                                      DenseBlasMatrixView<float> matrix,
                                      DenseBlasMatrixView<float> rhs,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries the exact GEQR/GELQ GETSLS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls both actual LWORK=-2 and -1 queries.
 * No numerical values, allocations or transfers are required by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
    LapackReport& report);

/** @brief Executes exact GEQR/GELQ GETSLS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status Getsls(const ReferenceLapackProvider& provider,
                                      DenseBlasTranspose transpose,
                                      DenseBlasMatrixView<double> matrix,
                                      DenseBlasMatrixView<double> rhs,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries the exact GEQR/GELQ GETSLS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls both actual LWORK=-2 and -1 queries.
 * No numerical values, allocations or transfers are required by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs, LapackReport& report);

/** @brief Executes exact GEQR/GELQ GETSLS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getsls(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
       DenseBlasMatrixView<std::complex<float>> matrix,
       DenseBlasMatrixView<std::complex<float>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries the exact GEQR/GELQ GETSLS workspace.
 * @param provider Explicit pinned CPU provider; borrowed for this call.
 * @param transpose N, or real T/complex C; other choices are rejected.
 * @param matrix Borrowed mutable m-by-n A; query leaves it unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves it unchanged.
 * @param report Initialized diagnostics; actual query INFO remains accessible.
 * @return Bound minimum/preferred scalar and explicit packing capacities, or
 * validation/provider failure. Calls both actual LWORK=-2 and -1 queries.
 * No numerical values, allocations or transfers are required by the query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetslsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs, LapackReport& report);

/** @brief Executes exact GEQR/GELQ GETSLS with caller storage.
 * @param provider Same pinned provider/build and ABI as the query.
 * @param transpose Same legal operation as the query.
 * @param matrix Borrowed overwritten A; full rank is assumed, not certified.
 * @param rhs Full output-capacity B; publication follows this file's contract.
 * @param plan Unmodified matching query result; stale metadata is rejected.
 * @param workspace Live typed scalar WORK and explicit A/B packing storage.
 * @param report Mandatory surviving raw INFO, outcome and output validity.
 * @return OK, numerical exact-zero diagonal failure, or validation/provider
 * error. Structural failures leave numerical outputs unchanged; partial
 * numerical outputs and provider-defect limitations are documented above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getsls(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
       DenseBlasMatrixView<std::complex<double>> matrix,
       DenseBlasMatrixView<std::complex<double>> rhs,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LEAST_SQUARES_H_
