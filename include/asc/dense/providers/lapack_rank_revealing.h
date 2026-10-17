#ifndef ASC_DENSE_PROVIDERS_LAPACK_RANK_REVEALING_H_
#define ASC_DENSE_PROVIDERS_LAPACK_RANK_REVEALING_H_

/** @file
 * @brief Exact reference GEQP3 column-pivoted QR and GELSY rank decisions.
 *
 * All calls are synchronous CPU-only, allocation-free and reentrant with
 * disjoint live operands, reports, plans, workspaces and provider objects.
 * Every operand and workspace region must be accessible to the explicitly
 * selected serial provider: host, not pinned/device/managed storage. Queries
 * call the actual LWORK=-1 route without reading numerical values or flags.
 * No fallback, transfer, synchronization or precision change is implicit.
 *
 * GEQP3 overwrites m-by-n A with raw LAPACK reflectors/R and min(m,n) tau.
 * On input each of n signed ASC64 JPVT flags is zero for a free column or any
 * nonzero value for a fixed column. On output JPVT is a one-based final column
 * permutation: column j of A_original*P is original column JPVT[j]-1, and
 * A_original*P = Q*R. It is not an LU swap sequence. Fixed columns retain their
 * relative order at the front; free columns need not retain their order.
 * The report's kColumnPivotedQr family must not be treated as an unpivoted
 * kHouseholderQr certificate. No numerical rank is certified by GEQP3.
 *
 * GELSY has no transpose option. B has exactly max(m,n) rows and nrhs columns;
 * only its first m rows are input, and only its first n rows are published as
 * the solution. All B layouts use mandatory caller column-major packing.
 * GELSY overwrites A with complete orthogonal factorization details, not a
 * reusable QR certificate: the auxiliary reflector data resides in WORK.
 * Its rank is the order of the selected leading triangular block, not a
 * singularity error or a certified rank of the original matrix. Ordinary
 * positive RCOND truncates trailing information by condition estimation.
 * All finite RCOND values are preserved, including nonpositive and greater
 * than one; nonpositive values do not request an automatic default tolerance.
 * Unusual cutoffs can admit singular divisions or nonfinite results.
 *
 * Pinned GELSY has a known fixed-column limitation: forcing a zero leading
 * column can return rank zero and a zero solution with INFO=0 even when a
 * free column gives a strictly better residual. More generally fixed columns
 * can defeat the leading-block rank assumption. No wrapper repair, implicit
 * pivot-policy change, original-matrix optimality certificate or universal
 * minimum-norm claim is made for such modes. Preserve original A/B and check
 * residuals, optimality and nullspace independently when those are required.
 *
 * GEQP3 with m=0,n>0 still enters LAPACK for its column permutation. Its
 * outer query returns one, but safe scalar capacity is at least n because
 * fixed columns reach a nested ORM/UNMQR validation before the quick return.
 * Both layouts require n live scalar caller layout slots with canonical
 * foreign LDA=1, so every zero-length SWAP's A(1,j) address has real backing.
 * Those slots, empty A and empty tau are unchanged. GEQP3 n=0 has no column
 * outputs and returns locally. GELSY empty execution returns rank zero and
 * leaves A/B/JPVT unchanged; nonempty all-zero A returns zero solution and
 * leaves original JPVT flags unchanged because no permutation was computed.
 * Empty queries remain actual foreign queries. Raw INFO is absent only for
 * documented local empty execution, never fabricated as zero.
 *
 * Scalar WORK, complex-only real WORK, ABI-width integer flags, ASC64 pivot
 * validation/widening and all A/B/tau packing are explicit caller regions
 * containing live aligned objects. Row-major A is packed; column-major A is
 * direct. GEQP3 tau and all JPVT outputs are staged. No padding is read or
 * published, and undocumented GELSY residual tails of B remain unchanged.
 *
 * Metadata, capacity, placement and alias failures precede numerical writes
 * and foreign entry. Report overlap is rejected without resetting aliased
 * storage; otherwise each call initializes its mandatory report. These
 * routines document no positive INFO: negative or impossible INFO, invalid
 * returned rank or invalid returned permutation is a provider defect. Exact
 * INFO survives, staged outputs are withheld, and direct column-major A may
 * be unusable. Successful GELSY reports kRankDecision, not kSingular; success
 * alone does not guarantee finite outputs or original-system optimality.
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

/** @brief Queries the exact GEQP3 workspace without numerical mutation.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n descriptor; no input values are read.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param tau Contiguous min(m,n)-entry scalar output descriptor.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound minimum/preferred work and packing requirements, or checked
 * validation/provider error. Fixed-flag values are not part of the query key.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> column_pivots, DenseBlasVectorView<float> tau,
    LapackReport& report);

/** @brief Executes exact column-pivoted QR with caller workspace.
 * @param provider Same explicit provider and integer ABI used by the query.
 * @param matrix Overwritten raw QR/R descriptor, with independent layout.
 * @param column_pivots Contiguous fixed flags replaced by the raw permutation.
 * @param tau Contiguous output reflectors; published after checked return.
 * @param plan Unmodified matching query plan; stale metadata is rejected.
 * @param workspace Live typed scalar, integer, conversion and packing regions.
 * @param report Surviving raw INFO, factor family and output validity.
 * @return OK or validation/provider error, with publication as documented
 * above. No rank or finite-result certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Geqp3(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<index_t> column_pivots,
                                     DenseBlasVectorView<float> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries the exact rank-revealing GELSY workspace.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n A descriptor; query reads no numerical values.
 * @param rhs Exactly max(m,n)-by-nrhs B, including full solution capacity.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param rcond Finite source rank cutoff; its exact bits bind the query key.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound safe minimum/preferred and all simultaneous auxiliary storage,
 * or validation/provider error. No allocation or rank computation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<index_t> column_pivots,
    float rcond, LapackReport& report);

/** @brief Executes exact GELSY, retaining its rank-decision limitations.
 * @param provider Same explicit provider/build and ABI as the query.
 * @param matrix Overwritten complete-orthogonal factorization details.
 * @param rhs Full-capacity B; only input rows are read and solution rows
 * written.
 * @param column_pivots Fixed flags replaced only when a permutation is
 * computed.
 * @param rcond Exact finite cutoff used by the query, including its sign bit.
 * @param rank Caller ASC64 rank output; unchanged on validation/provider
 * failure.
 * @param plan Matching unmodified query result.
 * @param workspace All typed work, integer and mandatory B packing regions.
 * @param report Raw INFO, kRankDecision and documented output validity.
 * @return OK or validation/provider error. Rank deficiency is not singular
 * failure; the fixed-column and cutoff caveats above remain applicable.
 */
ASC_DENSE_LAPACK_EXPORT Status Gelsy(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<index_t> column_pivots,
    float rcond, index_t& rank, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the exact GEQP3 workspace without numerical mutation.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n descriptor; no input values are read.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param tau Contiguous min(m,n)-entry scalar output descriptor.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound minimum/preferred work and packing requirements, or checked
 * validation/provider error. Fixed-flag values are not part of the query key.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> column_pivots, DenseBlasVectorView<double> tau,
    LapackReport& report);

/** @brief Executes exact column-pivoted QR with caller workspace.
 * @param provider Same explicit provider and integer ABI used by the query.
 * @param matrix Overwritten raw QR/R descriptor, with independent layout.
 * @param column_pivots Contiguous fixed flags replaced by the raw permutation.
 * @param tau Contiguous output reflectors; published after checked return.
 * @param plan Unmodified matching query plan; stale metadata is rejected.
 * @param workspace Live typed scalar, integer, conversion and packing regions.
 * @param report Surviving raw INFO, factor family and output validity.
 * @return OK or validation/provider error, with publication as documented
 * above. No rank or finite-result certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Geqp3(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<index_t> column_pivots,
                                     DenseBlasVectorView<double> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries the exact rank-revealing GELSY workspace.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n A descriptor; query reads no numerical values.
 * @param rhs Exactly max(m,n)-by-nrhs B, including full solution capacity.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param rcond Finite source rank cutoff; its exact bits bind the query key.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound safe minimum/preferred and all simultaneous auxiliary storage,
 * or validation/provider error. No allocation or rank computation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs, DenseBlasVectorView<index_t> column_pivots,
    double rcond, LapackReport& report);

/** @brief Executes exact GELSY, retaining its rank-decision limitations.
 * @param provider Same explicit provider/build and ABI as the query.
 * @param matrix Overwritten complete-orthogonal factorization details.
 * @param rhs Full-capacity B; only input rows are read and solution rows
 * written.
 * @param column_pivots Fixed flags replaced only when a permutation is
 * computed.
 * @param rcond Exact finite cutoff used by the query, including its sign bit.
 * @param rank Caller ASC64 rank output; unchanged on validation/provider
 * failure.
 * @param plan Matching unmodified query result.
 * @param workspace All typed work, integer and mandatory B packing regions.
 * @param report Raw INFO, kRankDecision and documented output validity.
 * @return OK or validation/provider error. Rank deficiency is not singular
 * failure; the fixed-column and cutoff caveats above remain applicable.
 */
ASC_DENSE_LAPACK_EXPORT Status Gelsy(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs, DenseBlasVectorView<index_t> column_pivots,
    double rcond, index_t& rank, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the exact GEQP3 workspace without numerical mutation.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n descriptor; no input values are read.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param tau Contiguous min(m,n)-entry scalar output descriptor.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound minimum/preferred work and packing requirements, or checked
 * validation/provider error. Fixed-flag values are not part of the query key.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> column_pivots,
    DenseBlasVectorView<std::complex<float>> tau, LapackReport& report);

/** @brief Executes exact column-pivoted QR with caller workspace.
 * @param provider Same explicit provider and integer ABI used by the query.
 * @param matrix Overwritten raw QR/R descriptor, with independent layout.
 * @param column_pivots Contiguous fixed flags replaced by the raw permutation.
 * @param tau Contiguous output reflectors; published after checked return.
 * @param plan Unmodified matching query plan; stale metadata is rejected.
 * @param workspace Live typed scalar, integer, conversion and packing regions.
 * @param report Surviving raw INFO, factor family and output validity.
 * @return OK or validation/provider error, with publication as documented
 * above. No rank or finite-result certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geqp3(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> column_pivots,
      DenseBlasVectorView<std::complex<float>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact rank-revealing GELSY workspace.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n A descriptor; query reads no numerical values.
 * @param rhs Exactly max(m,n)-by-nrhs B, including full solution capacity.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param rcond Finite source rank cutoff; its exact bits bind the query key.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound safe minimum/preferred and all simultaneous auxiliary storage,
 * or validation/provider error. No allocation or rank computation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasVectorView<index_t> column_pivots, float rcond,
    LapackReport& report);

/** @brief Executes exact GELSY, retaining its rank-decision limitations.
 * @param provider Same explicit provider/build and ABI as the query.
 * @param matrix Overwritten complete-orthogonal factorization details.
 * @param rhs Full-capacity B; only input rows are read and solution rows
 * written.
 * @param column_pivots Fixed flags replaced only when a permutation is
 * computed.
 * @param rcond Exact finite cutoff used by the query, including its sign bit.
 * @param rank Caller ASC64 rank output; unchanged on validation/provider
 * failure.
 * @param plan Matching unmodified query result.
 * @param workspace All typed work, integer and mandatory B packing regions.
 * @param report Raw INFO, kRankDecision and documented output validity.
 * @return OK or validation/provider error. Rank deficiency is not singular
 * failure; the fixed-column and cutoff caveats above remain applicable.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelsy(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasMatrixView<std::complex<float>> rhs,
      DenseBlasVectorView<index_t> column_pivots, float rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact GEQP3 workspace without numerical mutation.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n descriptor; no input values are read.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param tau Contiguous min(m,n)-entry scalar output descriptor.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound minimum/preferred work and packing requirements, or checked
 * validation/provider error. Fixed-flag values are not part of the query key.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqp3Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> column_pivots,
    DenseBlasVectorView<std::complex<double>> tau, LapackReport& report);

/** @brief Executes exact column-pivoted QR with caller workspace.
 * @param provider Same explicit provider and integer ABI used by the query.
 * @param matrix Overwritten raw QR/R descriptor, with independent layout.
 * @param column_pivots Contiguous fixed flags replaced by the raw permutation.
 * @param tau Contiguous output reflectors; published after checked return.
 * @param plan Unmodified matching query plan; stale metadata is rejected.
 * @param workspace Live typed scalar, integer, conversion and packing regions.
 * @param report Surviving raw INFO, factor family and output validity.
 * @return OK or validation/provider error, with publication as documented
 * above. No rank or finite-result certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geqp3(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> column_pivots,
      DenseBlasVectorView<std::complex<double>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact rank-revealing GELSY workspace.
 * @param provider Explicit borrowed pinned CPU provider.
 * @param matrix Mutable m-by-n A descriptor; query reads no numerical values.
 * @param rhs Exactly max(m,n)-by-nrhs B, including full solution capacity.
 * @param column_pivots Contiguous n-entry ASC64 input flags/output permutation.
 * @param rcond Finite source rank cutoff; its exact bits bind the query key.
 * @param report Initialized diagnostics, including actual foreign query INFO.
 * @return Bound safe minimum/preferred and all simultaneous auxiliary storage,
 * or validation/provider error. No allocation or rank computation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsyWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasVectorView<index_t> column_pivots, double rcond,
    LapackReport& report);

/** @brief Executes exact GELSY, retaining its rank-decision limitations.
 * @param provider Same explicit provider/build and ABI as the query.
 * @param matrix Overwritten complete-orthogonal factorization details.
 * @param rhs Full-capacity B; only input rows are read and solution rows
 * written.
 * @param column_pivots Fixed flags replaced only when a permutation is
 * computed.
 * @param rcond Exact finite cutoff used by the query, including its sign bit.
 * @param rank Caller ASC64 rank output; unchanged on validation/provider
 * failure.
 * @param plan Matching unmodified query result.
 * @param workspace All typed work, integer and mandatory B packing regions.
 * @param report Raw INFO, kRankDecision and documented output validity.
 * @return OK or validation/provider error. Rank deficiency is not singular
 * failure; the fixed-column and cutoff caveats above remain applicable.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelsy(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasMatrixView<std::complex<double>> rhs,
      DenseBlasVectorView<index_t> column_pivots, double rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_RANK_REVEALING_H_
