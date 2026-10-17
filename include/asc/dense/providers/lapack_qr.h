#ifndef ASC_DENSE_PROVIDERS_LAPACK_QR_H_
#define ASC_DENSE_PROVIDERS_LAPACK_QR_H_

/** @file
 * @brief Explicit reference GEQRF/GEQR2 and raw ORG/UNG/ORM/UNM QR experts.
 *
 * These Dense-owned optional symbols require ASC::dense_lapack and an explicit
 * checked ReferenceLapackProvider. All operands and workspace must be
 * accessible to its serial CPU context. Both row/column layouts are supported,
 * with caller kLayoutConversion storage for each row-major matrix and kScalar
 * storage for foreign WORK. All storage contains live correctly aligned scalar
 * objects. Tau is contiguous with increment one. No allocation, transfer,
 * implicit fallback, synchronization or densification occurs.
 *
 * GEQRF/GEQR2 overwrite full A and tau. ORGQR/UNGQR overwrite their m-by-n A
 * with the first n columns of Q for every 0<=k<=n<=m, where k=tau.size().
 * Only strict lower tails in the first k columns are numerical inputs; implicit
 * unit diagonals, upper entries and output-only columns are not read during
 * packing. ORMQR/UNMQR preserve raw order-by-k reflectors and tau, applying
 * their product to C on either side. Real transpose is N/T; complex is N/C.
 * Raw reflector provenance and absence of concurrent mutation are caller
 * responsibilities. Partial k is intentional and does not require a successful
 * whole-factor certificate. Success is not a rank, finiteness or accuracy
 * certificate.
 *
 * GEQRF and Q experts query actual pinned LAPACK with LWORK=-1 and a local
 * scalar WORK(1); query-only source paths do not read numerical operands or
 * mutate A/tau/C. GEQR2 has no foreign query: its query computes WORK(N).
 * Reports distinguish real query calls from formula-only queries. Plans bind
 * scalar/routine/provider, shapes, flags, original/effective leading
 * dimensions, layouts, tau length/increment and capacities. Execution validates
 * freshness without calling the foreign query again. Minimum WORK is max(1,n)
 * for GEQRF except empty factors need one, N for GEQR2, max(1,n) for
 * generation, and max(1,C.columns()) for left / max(1,C.rows()) for right
 * application.
 *
 * The pinned preferred queries use NB=32, and application adds fixed
 * TSIZE=65*64=4160. Integer products and floating query conversions are checked
 * before foreign execution, including S/C SROUNDUP_LWORK's integer
 * reconversion. ASC row strides and layout storage retain ASC's width; only
 * effective foreign arguments use the provider integer limit. Query preferred
 * values are checked against the exact source-pinned formula, not trusted
 * blindly. Nonempty calls additionally check row/column terminal increments
 * and reflector-loop increments (one for GEQR2, up to 32 otherwise).
 *
 * Operand reachable spans, all supplied workspace regions and live
 * provider/plan/workspace/report metadata must be disjoint. Validation precedes
 * packing and writes. Reports reset before validation except an aliased report
 * is rejected unchanged. Structural failures leave absent INFO and unchanged
 * numerical outputs. Any nonzero INFO is a provider defect (these routines
 * have no positive numerical INFO), preserved exactly; packed outputs are not
 * published and direct outputs are unusable. Successful factor execution alone
 * receives the Householder-QR family tag.
 *
 * Empty execution validates all capacities but skips the foreign call, with
 * absent INFO. Generation with k=0 and n>0 still executes to form identity
 * columns. Application with k=0 leaves C unchanged. Query calls still execute
 * their actual query even for empty operands. Independent disjoint calls are
 * reentrant; all borrowed storage must remain alive for the complete call.
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

/** @brief Queries single-real GEQRF through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<float> tau, LapackReport& report);
/** @brief Executes single-real GEQRF with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Geqrf(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<float> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single-real GEQR2 by checked formulas only.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<float> tau, LapackReport& report);
/** @brief Executes single-real GEQR2 with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Geqr2(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<float> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single-real ORGQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryOrgqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const float> tau, LapackReport& report);
/** @brief Executes single-real ORGQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Orgqr(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<const float> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single-real ORMQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryOrmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose, DenseBlasMatrixView<const float> reflectors,
    DenseBlasVectorView<const float> tau, DenseBlasMatrixView<float> matrix,
    LapackReport& report);
/** @brief Executes single-real ORMQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ormqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
      DenseBlasTranspose transpose, DenseBlasMatrixView<const float> reflectors,
      DenseBlasVectorView<const float> tau, DenseBlasMatrixView<float> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double-real GEQRF through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> tau, LapackReport& report);
/** @brief Executes double-real GEQRF with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Geqrf(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<double> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double-real GEQR2 by checked formulas only.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> tau, LapackReport& report);
/** @brief Executes double-real GEQR2 with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Geqr2(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<double> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double-real ORGQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryOrgqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const double> tau, LapackReport& report);
/** @brief Executes double-real ORGQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Orgqr(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<const double> tau,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double-real ORMQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryOrmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose, DenseBlasMatrixView<const double> reflectors,
    DenseBlasVectorView<const double> tau, DenseBlasMatrixView<double> matrix,
    LapackReport& report);
/** @brief Executes double-real ORMQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status Ormqr(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose, DenseBlasMatrixView<const double> reflectors,
    DenseBlasVectorView<const double> tau, DenseBlasMatrixView<double> matrix,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries single-complex GEQRF through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> tau, LapackReport& report);
/** @brief Executes single-complex GEQRF with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geqrf(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<std::complex<float>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single-complex GEQR2 by checked formulas only.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> tau, LapackReport& report);
/** @brief Executes single-complex GEQR2 with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geqr2(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<std::complex<float>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single-complex UNGQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryUngqrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const std::complex<float>> tau, LapackReport& report);
/** @brief Executes single-complex UNGQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ungqr(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<const std::complex<float>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single-complex UNMQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryUnmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<float>> reflectors,
    DenseBlasVectorView<const std::complex<float>> tau,
    DenseBlasMatrixView<std::complex<float>> matrix, LapackReport& report);
/** @brief Executes single-complex UNMQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Unmqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
      DenseBlasTranspose transpose,
      DenseBlasMatrixView<const std::complex<float>> reflectors,
      DenseBlasVectorView<const std::complex<float>> tau,
      DenseBlasMatrixView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double-complex GEQRF through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> tau, LapackReport& report);
/** @brief Executes double-complex GEQRF with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geqrf(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<std::complex<double>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double-complex GEQR2 by checked formulas only.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeqr2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> tau, LapackReport& report);
/** @brief Executes double-complex GEQR2 with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n factor input/output; query leaves it unchanged.
 * @param tau Contiguous output of exact length min(m,n); query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geqr2(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<std::complex<double>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double-complex UNGQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryUngqrWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const std::complex<double>> tau, LapackReport& report);
/** @brief Executes double-complex UNGQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param matrix Mutable m-by-n reflector input/Q output; query leaves it
 * unchanged.
 * @param tau Contiguous input of length k with 0<=k<=n<=m; query leaves it
 * unchanged.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Ungqr(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<const std::complex<double>> tau,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double-complex UNMQR through its actual foreign query.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param report Mandatory query diagnostics with exact INFO only if called.
 * @return Checked complete workspace plan or structural/provider failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryUnmqrWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasSide side,
    DenseBlasTranspose transpose,
    DenseBlasMatrixView<const std::complex<double>> reflectors,
    DenseBlasVectorView<const std::complex<double>> tau,
    DenseBlasMatrixView<std::complex<double>> matrix, LapackReport& report);
/** @brief Executes double-complex UNMQR with caller-owned storage.
 * @param provider Explicit checked serial reference provider.
 * @param side Left/right multiplication.
 * @param transpose Real N/T or complex N/C operation.
 * @param reflectors Borrowed order-by-k raw strict-lower Householder tails.
 * @param tau Borrowed contiguous k scalar factors.
 * @param matrix Mutable C; query preserves all entries.
 * @param plan Unmodified matching query result.
 * @param workspace Disjoint live scalar WORK and explicit packing buffers.
 * @param report Mandatory diagnostics; see file publication contract.
 * @return OK or structural/provider failure; no rank certificate is implied.
 */
ASC_DENSE_LAPACK_EXPORT Status
Unmqr(const ReferenceLapackProvider& provider, DenseBlasSide side,
      DenseBlasTranspose transpose,
      DenseBlasMatrixView<const std::complex<double>> reflectors,
      DenseBlasVectorView<const std::complex<double>> tau,
      DenseBlasMatrixView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_QR_H_
