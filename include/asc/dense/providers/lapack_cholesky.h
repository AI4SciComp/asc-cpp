#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_H_

/** @file
 * @brief Checked full-storage reference Cholesky algorithms, solve and inverse.
 *
 * These optional Dense-owned symbols require ASC::dense_lapack and an explicit
 * ReferenceLapackProvider. All operands must be accessible to its CPU context.
 * Both layouts and independent A/B layouts are supported. The selected triangle
 * alone is input/output; the opposite triangle and padding are never accessed.
 * Hermitian factorization input uses only real diagonal components. POTRS and
 * POTRI instead consume actual triangular factor data, including complex
 * diagonal components; those are not normalized as Hermitian input.
 *
 * None of these six upstream routines has foreign WORK or a query mode.
 * Queries perform structural checks and compute exact minimum=preferred caller
 * packing capacities, without reading numerical entries or calling LAPACK.
 * kLayoutConversion requires n*n live scalar objects for each row-major factor
 * operand, plus n*nrhs for row-major B. Column-major operands require no
 * packing. Plans bind routine, scalar, both original/effective leading
 * dimensions, shapes, layouts, triangle and complete provider identity.
 * Original row-major strides remain ASC-sized identity options; only actual
 * foreign leading dimensions are narrowed to the provider integer width. An
 * empty POTRS still validates its plan, but neither packs nor reads its factor.
 * Checked provider arithmetic also requires n+64 to fit for POTRF/POSV/POTRI
 * (the pinned PO/TRF, TR/TRI and LA/UUM block size is 64), and n+1 otherwise.
 * POTF2/POTRI require n*effective_lda to fit for strided provider INTEGER
 * vector cursors. For n>0, nrhs+1 must fit. These bounds do not repurpose the
 * provider integer width as a bound on ASC-owned layout packing byte counts.
 *
 * Workspace, operands and live provider/plan/workspace/report metadata must be
 * disjoint. All preflight checks precede packing, writes and foreign calls.
 * Reports reset before structural checks except an aliased report is rejected
 * untouched to avoid corrupting another live input. Structural failures leave
 * absent INFO, called_provider=false and unchanged numerical outputs.
 *
 * Positive factorization/POSV INFO returns kNumerical, identifies the
 * zero-based failed leading minor and publishes selected partial factor data.
 * POSV leaves B unchanged then. For row-major complex positive-INFO
 * factorization, selected offdiagonals and real diagonal components are
 * published; original ignored diagonal imaginary components are preserved. This
 * is not byte equivalence with ignored imaginary output of column-major
 * upstream execution. Successful factorization produces real complex diagonals
 * and a reusable Cholesky report. POTRI positive INFO denotes an exact zero
 * factor diagonal; A stays unchanged. Successful POTRI is an inverse, not a
 * factor, and has no factor-family tag. Negative or out-of-range INFO is a
 * provider defect; no packed result is published and direct column-major
 * outputs are marked unusable.
 *
 * Empty n=0 execution succeeds without a foreign call or fabricated INFO.
 * POSV with n>0 and nrhs=0 still factors A. POTRS with nrhs=0 succeeds without
 * foreign execution. Source-defined floating-point behavior applies: success
 * does not establish finiteness, conditioning or forward accuracy. Raw POTRI
 * factor provenance is the caller's responsibility. A borrowed POTRS factor
 * must remain unmodified since the matching successful factorization.
 * No allocation, implicit transfer, densification, fallback or synchronization
 * occurs. Caller-owned live storage covers the entire call; independent
 * disjoint calls are reentrant.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real POTRF packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix);
/** @brief Executes single real POTRF through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potrf(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<float> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real POTRF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix);
/** @brief Executes single real POTRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potrf2(const ReferenceLapackProvider& provider,
                                      DenseBlasTriangle triangle,
                                      DenseBlasMatrixView<float> matrix,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries single real POTF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix);
/** @brief Executes single real POTF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potf2(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<float> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real POTRI packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix);
/** @brief Executes single real POTRI through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<float> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real POTRS packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<float> factor, DenseBlasMatrixView<float> rhs);
/** @brief Executes single real POTRS through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status Potrs(const ReferenceLapackProvider& provider,
                                     LapackCholeskyFactorView<float> factor,
                                     DenseBlasMatrixView<float> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real POSV packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasMatrixView<float> rhs);
/** @brief Executes single real POSV through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Posv(const ReferenceLapackProvider& provider,
                                    DenseBlasTriangle triangle,
                                    DenseBlasMatrixView<float> matrix,
                                    DenseBlasMatrixView<float> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Queries double real POTRF packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix);
/** @brief Executes double real POTRF through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potrf(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<double> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real POTRF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix);
/** @brief Executes double real POTRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potrf2(const ReferenceLapackProvider& provider,
                                      DenseBlasTriangle triangle,
                                      DenseBlasMatrixView<double> matrix,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries double real POTF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix);
/** @brief Executes double real POTF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potf2(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<double> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real POTRI packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix);
/** @brief Executes double real POTRI through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Potri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasMatrixView<double> matrix,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real POTRS packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<double> factor, DenseBlasMatrixView<double> rhs);
/** @brief Executes double real POTRS through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status Potrs(const ReferenceLapackProvider& provider,
                                     LapackCholeskyFactorView<double> factor,
                                     DenseBlasMatrixView<double> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real POSV packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasMatrixView<double> rhs);
/** @brief Executes double real POSV through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status Posv(const ReferenceLapackProvider& provider,
                                    DenseBlasTriangle triangle,
                                    DenseBlasMatrixView<double> matrix,
                                    DenseBlasMatrixView<double> rhs,
                                    const LapackWorkspacePlan& plan,
                                    const LapackWorkspace& workspace,
                                    LapackReport& report);

/** @brief Queries single complex POTRF packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix);
/** @brief Executes single complex POTRF through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex POTRF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix);
/** @brief Executes single complex POTRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potrf2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<float>> matrix,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries single complex POTF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix);
/** @brief Executes single complex POTF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potf2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex POTRI packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix);
/** @brief Executes single complex POTRI through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex POTRS packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes single complex POTRS through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potrs(const ReferenceLapackProvider& provider,
      LapackCholeskyFactorView<std::complex<float>> factor,
      DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single complex POSV packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes single complex POSV through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Posv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<std::complex<float>> matrix,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries double complex POTRF packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix);
/** @brief Executes double complex POTRF through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potrf(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex POTRF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix);
/** @brief Executes double complex POTRF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potrf2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
       DenseBlasMatrixView<std::complex<double>> matrix,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries double complex POTF2 packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotf2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix);
/** @brief Executes double complex POTF2 through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potf2(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex POTRI packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix);
/** @brief Executes double complex POTRI through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square raw triangular factor/inverse; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasMatrixView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex POTRS packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPotrsWorkspace(
    const ReferenceLapackProvider& provider,
    LapackCholeskyFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes double complex POTRS through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param factor Unmodified successful same-provider Cholesky factor.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Status
Potrs(const ReferenceLapackProvider& provider,
      LapackCholeskyFactorView<std::complex<double>> factor,
      DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex POSV packing without foreign execution.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @return Exact checked conversion plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPosvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes double complex POSV through the selected provider.
 * @param provider Explicit checked reference provider.
 * @param triangle Selected stored upper/lower triangle.
 * @param matrix Square symmetric/Hermitian input/factor; query leaves it
 * unchanged.
 * @param rhs Disjoint n-by-nrhs input/output; query leaves it unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Caller-owned disjoint live conversion storage.
 * @param report Failure-surviving diagnostics; see file publication contract.
 * @return OK, structural/provider failure, or kNumerical for the documented
 * positive INFO.
 */
ASC_DENSE_LAPACK_EXPORT Status
Posv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<std::complex<double>> matrix,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_H_
