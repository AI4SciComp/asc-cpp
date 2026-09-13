#ifndef ASC_DENSE_PROVIDERS_LAPACK_SVD_LEAST_SQUARES_H_
#define ASC_DENSE_PROVIDERS_LAPACK_SVD_LEAST_SQUARES_H_

/** @file
 * @brief Exact pinned SVD least-squares drivers with explicit caller storage.
 *
 * GELSS uses bidiagonal QR iteration; GELSD uses divide-and-conquer. They are
 * distinct named algorithms, never substituted for one another. Both accept
 * rank-deficient A. A is m-by-n, B has exactly max(m,n) rows and nrhs columns,
 * and singular_values has exactly min(m,n) contiguous underlying-real entries.
 * Only the first m rows of B are input. Successful B's first n rows contain
 * minimum-norm least-squares solutions for the source-selected numerical rank.
 *
 * Every finite RCOND is passed unchanged. GELSS uses machine precision when
 * RCOND<0 and applies the source safe-minimum floor. GELSD's nested LALSD
 * actually uses machine epsilon for RCOND<=0 or RCOND>=1, despite narrower
 * top-level prose; its scalar subproblem has its own exact-zero rank rule.
 * Neither INFO=0 nor rank output certifies finite results for arbitrary data.
 * Rank truncation solves the truncated problem, not necessarily the unmodified
 * full-rank problem. Preserve original A/B for independent residual checks.
 * Before mutation, nonempty execution checks finite full A and (unless A is
 * all zero) the first m input B rows. A nonfinite meaningful input returns
 * kNumerical with kNotRun/kUnchanged and absent INFO; it is not foreign
 * nonconvergence. The pinned GELSD otherwise can stop in nested LASCL.
 * Queries read no numerical input. Empty and all-zero-A source quick returns
 * do not inspect ignored B; S, output-only B slots and padding are never
 * finiteness inputs. Finite inputs alone do not certify finite computed output.
 *
 * CPU-only, reentrant with disjoint live operands and metadata. No allocation,
 * implicit transfer, synchronization, precision change or provider selection
 * occurs. The serial reference context admits host storage, including every
 * nonempty workspace region. Caller regions must contain live aligned objects
 * of the role's documented scalar/real/private-integer types.
 * S/DGELSD's checked minimum can exceed its source MINWRK: some wide cases
 * otherwise enter GEBRD without its required N entries. The plan admits the
 * smaller safe threshold for the fallback or complete source Path2a; the
 * actual source preferred query is retained, without substituting algorithms.
 *
 * Row-major A is explicitly packed; every B uses caller column-major storage.
 * Only input B rows are read. GELSS NRHS=0 still computes singular values/rank;
 * its caller B scratch includes max(m,n) live surrogate entries for upstream
 * zero-length row arguments. S is staged separately, and foreign rank is
 * checked before widening. Padding and undocumented B tails remain unchanged.
 * When m>=n and rank=n, both drivers also publish trailing m-n transformed
 * residual coordinates; these may retain source scaling and are not residual
 * vectors. Other successful calls publish only n solution rows.
 *
 * A is overwritten with the actual upstream state, not a certified factor.
 * In particular, GELSS's wide high-workspace Path2a leaves LQ factors in A,
 * not the rowwise right singular vectors promised by its top-level prose.
 * No V^H is fabricated. Full named-output coverage of this source discrepancy
 * remains an explicit incomplete gate, independently of solution accuracy.
 *
 * Structural failures precede numerical writes and foreign execution. Positive
 * source-valid INFO in [1,min(m,n)-1] is nonconvergence: raw transformed A,
 * first m B rows and intermediate S
 * are published, possibly still scaled; they are not a solution or singular-
 * value certificate, and rank is unchanged. No diagnostic index is invented
 * from an off-diagonal count. Impossible positive INFO, negative INFO and
 * provider defects preserve raw INFO,
 * withhold packed outputs and may leave direct column-major A unusable.
 * Report overlap rejects without resetting the aliased object; otherwise all
 * calls initialize the mandatory report before validation.
 *
 * Empty m/n execution returns rank zero and leaves A/B/S unchanged, matching
 * the source quick return. Queries remain actual foreign calls; DGELSD has
 * distinct nontrivial empty workspace requirements. GELSD with NRHS=0 and
 * nonzero A is rejected before mutation as unsupported: the pinned routine
 * otherwise reaches LALSD's global error handler. Zero A and empty shapes
 * retain their separately checked safe source paths. This source mode remains
 * incomplete and does not receive full capability credit.
 * Single-real/complex GELSD also rejects nonzero A with min(m,n)>=212992:
 * the attested source's single-REAL divide-tree logarithm can create a leaf
 * exceeding its reserved U/V slices. Input-dependent subproblems require the
 * bound even when the full matrix's logarithm is benign. Actual queries and
 * all-zero A remain admitted after the ordinary arithmetic/storage checks.
 * These larger nonzero modes are explicitly incomplete, not alternate-driver
 * fallbacks or claimed complete SVD capability.
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

/** @brief Queries the exact single-real bidiagonal-QR GELSS workspace.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) real output capacity, not read.
 * @param rcond Finite rank cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Checked minimum/preferred and explicit packing/staging capacities,
 * or structural/provider failure. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<float> singular_values,
    float rcond, LapackReport& report);

/** @brief Executes single-real GELSS using the file-level mutation contract.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A, not a certified reusable factor.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous real output, staged before publication.
 * @param rcond Finite cutoff matching the plan's exact bit pattern.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Live typed scalar WORK and explicit B/A/S caller storage.
 * @param report Mandatory surviving raw INFO and output-validity diagnostics.
 * @return OK, numerical nonconvergence, structural failure or provider defect;
 * structural failures preserve all numerical outputs without foreign entry.
 */
ASC_DENSE_LAPACK_EXPORT Status Gelss(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<float> singular_values,
    float rcond, index_t& rank, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the exact double-real bidiagonal-QR GELSS workspace.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) real output capacity, not read.
 * @param rcond Finite rank cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Checked minimum/preferred and explicit packing/staging capacities,
 * or structural/provider failure. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report);

/** @brief Executes double-real GELSS using the file-level mutation contract.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A, not a certified reusable factor.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous real output, staged before publication.
 * @param rcond Finite cutoff matching the plan's exact bit pattern.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Live typed scalar WORK and explicit B/A/S caller storage.
 * @param report Mandatory surviving raw INFO and output-validity diagnostics.
 * @return OK, numerical nonconvergence, structural failure or provider defect;
 * structural failures preserve all numerical outputs without foreign entry.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelss(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
      DenseBlasVectorView<double> singular_values, double rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact single-complex bidiagonal-QR GELSS workspace.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) underlying-real capacity, not
 * read.
 * @param rcond Finite rank cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Checked complex/real WORK and explicit packing/staging capacities,
 * or structural/provider failure. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasVectorView<float> singular_values, float rcond,
    LapackReport& report);

/** @brief Executes single-complex GELSS with real singular-value output.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A, not a certified reusable factor.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous underlying-real output, staged explicitly.
 * @param rcond Finite cutoff matching the plan's exact bit pattern.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Live complex/real WORK and explicit B/A/S caller storage.
 * @param report Mandatory surviving raw INFO and output-validity diagnostics.
 * @return OK, numerical nonconvergence, structural failure or provider defect;
 * structural failures preserve all numerical outputs without foreign entry.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelss(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasMatrixView<std::complex<float>> rhs,
      DenseBlasVectorView<float> singular_values, float rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries the exact double-complex bidiagonal-QR GELSS workspace.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) underlying-real capacity, not
 * read.
 * @param rcond Finite rank cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Checked complex/real WORK and explicit packing/staging capacities,
 * or structural/provider failure. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelssWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report);

/** @brief Executes double-complex GELSS with real singular-value output.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A, not a certified reusable factor.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous underlying-real output, staged explicitly.
 * @param rcond Finite cutoff matching the plan's exact bit pattern.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Live complex/real WORK and explicit B/A/S caller storage.
 * @param report Mandatory surviving raw INFO and output-validity diagnostics.
 * @return OK, numerical nonconvergence, structural failure or provider defect;
 * structural failures preserve all numerical outputs without foreign entry.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelss(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasMatrixView<std::complex<double>> rhs,
      DenseBlasVectorView<double> singular_values, double rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single-real divide-and-conquer GELSD workspace.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) real capacity, not read.
 * @param rcond Finite source cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Scalar/integer WORK and explicit packing/staging capacities, or
 * structural/provider failure. The safe query does not certify executable
 * nonzero-A/zero-RHS mode. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<float> singular_values,
    float rcond, LapackReport& report);

/** @brief Executes single-real GELSD using the source cutoff semantics.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A; nonzero-A/zero-RHS execution is
 * unsupported.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous real output, staged before publication.
 * @param rcond Finite cutoff matching the plan, including source overrides.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Live scalar/integer WORK and explicit B/A/S caller storage.
 * @param report Mandatory raw INFO, rank-decision/nonconvergence and validity.
 * @return OK, nonconvergence, unsupported mode or structural/provider failure;
 * preflight errors preserve numerical destinations without foreign execution.
 */
ASC_DENSE_LAPACK_EXPORT Status Gelsd(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasMatrixView<float> rhs, DenseBlasVectorView<float> singular_values,
    float rcond, index_t& rank, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double-real GELSD, including its distinct empty requirements.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) real capacity, not read.
 * @param rcond Finite source cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Scalar/integer WORK and explicit packing/staging capacities, or
 * structural/provider failure. The safe query does not certify executable
 * nonzero-A/zero-RHS mode. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasMatrixView<double> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report);

/** @brief Executes double-real GELSD using the source cutoff semantics.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A; nonzero-A/zero-RHS execution is
 * unsupported.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous real output, staged before publication.
 * @param rcond Finite cutoff matching the plan, including source overrides.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Live scalar/integer WORK and explicit B/A/S caller storage.
 * @param report Mandatory raw INFO, rank-decision/nonconvergence and validity.
 * @return OK, nonconvergence, unsupported mode or structural/provider failure;
 * preflight errors preserve numerical destinations without foreign execution.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelsd(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs,
      DenseBlasVectorView<double> singular_values, double rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single-complex GELSD's scalar, real and integer work arrays.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) underlying-real capacity, not
 * read.
 * @param rcond Finite source cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Three WORK roles and explicit packing/staging capacities, or
 * structural/provider failure. The safe query does not certify executable
 * nonzero-A/zero-RHS mode. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs,
    DenseBlasVectorView<float> singular_values, float rcond,
    LapackReport& report);

/** @brief Executes single-complex GELSD with real singular-value output.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A; nonzero-A/zero-RHS execution is
 * unsupported.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous underlying-real output, staged explicitly.
 * @param rcond Finite cutoff matching the plan, including source overrides.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Complex/real/integer WORK and explicit B/A/S caller storage.
 * @param report Mandatory raw INFO, rank-decision/nonconvergence and validity.
 * @return OK, nonconvergence, unsupported mode or structural/provider failure;
 * preflight errors preserve numerical destinations without foreign execution.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelsd(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasMatrixView<std::complex<float>> rhs,
      DenseBlasVectorView<float> singular_values, float rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double-complex GELSD's scalar, real and integer work arrays.
 * @param provider Borrowed explicit pinned CPU provider.
 * @param matrix Borrowed m-by-n A; query leaves all values unchanged.
 * @param rhs Borrowed max(m,n)-by-nrhs B; query leaves all values unchanged.
 * @param singular_values Contiguous min(m,n) underlying-real capacity, not
 * read.
 * @param rcond Finite source cutoff; exact bits bind the returned plan.
 * @param report Mandatory initialized report retaining actual query INFO.
 * @return Three WORK roles and explicit packing/staging capacities, or
 * structural/provider failure. The safe query does not certify executable
 * nonzero-A/zero-RHS mode. No allocation or numerical input reads.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGelsdWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs,
    DenseBlasVectorView<double> singular_values, double rcond,
    LapackReport& report);

/** @brief Executes double-complex GELSD with real singular-value output.
 * @param provider Same borrowed pinned CPU provider as the plan.
 * @param matrix Overwritten m-by-n A; nonzero-A/zero-RHS execution is
 * unsupported.
 * @param rhs Full output-capacity B; only its first m rows are read.
 * @param singular_values Contiguous underlying-real output, staged explicitly.
 * @param rcond Finite cutoff matching the plan, including source overrides.
 * @param rank Caller ASC64 output; published only after successful execution.
 * @param plan Unmodified query result matching shape/options/provider/ABI.
 * @param workspace Complex/real/integer WORK and explicit B/A/S caller storage.
 * @param report Mandatory raw INFO, rank-decision/nonconvergence and validity.
 * @return OK, nonconvergence, unsupported mode or structural/provider failure;
 * preflight errors preserve numerical destinations without foreign execution.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gelsd(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasMatrixView<std::complex<double>> rhs,
      DenseBlasVectorView<double> singular_values, double rcond, index_t& rank,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_SVD_LEAST_SQUARES_H_
