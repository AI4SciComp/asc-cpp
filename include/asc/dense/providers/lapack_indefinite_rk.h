#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_H_

/** @file
 * @brief Explicit RK symmetric/Hermitian factors with separate D offdiagonal.
 *
 * SYTF2_RK/SYTRF_RK compute A=P*U*D*U^T*P^T or P*L*D*L^T*P^T.
 * HETF2_RK/HETRF_RK use conjugate transposes. A stores D's diagonal and the
 * strict triangle of unit U/L; E stores its 2-by-2 block offdiagonals. Using
 * one-based indices, upper E(i)=D(i-1,i), E(1)=0; lower E(i)=D(i+1,i), E(n)=0.
 * E(k)=0 for 1-by-1 blocks and for the unused partner of each 2-by-2 block.
 * The corresponding selected offdiagonal position in A is zero for a 2-block.
 * E has the SAME scalar type as A, including complex Hermitian operations.
 *
 * Signed one-based pivots encode independent adjacent negative pairs. Apply
 * upper interchanges in descending index order, lower in ascending order;
 * each index i exchanges with abs(IPIV(i)). The absolute target is at most i
 * for upper, at least i for lower. The report's kRook family identifies this
 * pivot protocol, not interchangeable factors: existing ROOK factor views do
 * not accept RK provenance or storage. Preserve A, E and pivots together.
 *
 * These synchronous serial CPU operations use only the explicit optional
 * provider. They allocate nothing, transfer nothing and change no global
 * state. All numerical storage must be host/pinned and context-accessible.
 * Independent calls require distinct writable operands, workspace and reports.
 *
 * Queries read only metadata and never call LAPACK. Active calls require n
 * provider-width kInteger entries in caller byte storage; execution begins
 * their trivial lifetimes. TF2 needs no scalar WORK. TRF needs minimum one
 * scalar entry, preferred pinned NB=64 times n with actual S/C upward rounding.
 * Execution caps supplied capacity at preferred and preserves native block-size
 * reduction. Row-major A and ALL original Hermitian A require n*n live scalar
 * kLayoutConversion entries. Hermitian packing reads real diagonals and
 * selected offdiagonals; complex symmetric packing preserves full selected
 * coefficients. E is persistent caller output, not workspace. Empty n=0
 * requires no workspace or array access and makes no native call. This avoids
 * the pinned unblocked native empty-order E write, whose separate failed guard
 * evidence is retained.
 *
 * Plans bind routine/scalar/provider/ABI, n, triangle, symmetry, layouts,
 * original/effective leading dimensions and vector sizes/increments. Original
 * metadata, foreign loop/cursor counts, workspace products and byte totals are
 * checked. A, E, pivots and scratch must be mutually disjoint and separate from
 * provider/plan/workspace/report metadata. All storage and metadata must remain
 * live and unmodified during a call. Metadata aliases leave report unchanged;
 * other preflight errors reset report without touching numerical arrays.
 *
 * Full-width INFO is seeded and checked. INFO=0 means native completion, not
 * finiteness or good conditioning. INFO>0 preserves completed selected factors,
 * E and checked pivots as documented partial output, with diagnostic_index
 * INFO-1. Exact-zero reported diagonal gives kSingular; other nonfinite source
 * cases retain kPartialResult. No scan substitutes a different native INFO.
 * Unexpected/unwritten INFO, invalid pivots or invalid structural-zero storage
 * give a provider defect. Packed A and public pivots are withheld then; direct
 * column-major symmetric A and E may already have changed. All outputs are
 * unusable after a provider defect. Normal publication preserves A's opposite
 * triangle and padding and both vectors' padding. No numerical normalization
 * or fallback algorithm is applied to native output.
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

/** @brief Queries single real SYTF2_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single real SYTF2_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status Sytf2Rk(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYTF2_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double real SYTF2_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status Sytf2Rk(const ReferenceLapackProvider& provider,
                                       DenseBlasTriangle triangle,
                                       DenseBlasMatrixView<double> matrix,
                                       DenseBlasVectorView<double> off_diagonal,
                                       DenseBlasVectorView<index_t> pivots,
                                       const LapackWorkspacePlan& plan,
                                       const LapackWorkspace& workspace,
                                       LapackReport& report);

/** @brief Queries single complex SYTF2_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single complex SYTF2_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytf2Rk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<float>> matrix,
        DenseBlasVectorView<std::complex<float>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex SYTF2_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double complex SYTF2_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytf2Rk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<double>> matrix,
        DenseBlasVectorView<std::complex<double>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex Hermitian HETF2_RK storage without array
 * reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single complex Hermitian HETF2_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetf2Rk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<float>> matrix,
        DenseBlasVectorView<std::complex<float>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Hermitian HETF2_RK storage without array
 * reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetf2RkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double complex Hermitian HETF2_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetf2Rk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<double>> matrix,
        DenseBlasVectorView<std::complex<double>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single real SYTRF_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single real SYTRF_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status SytrfRk(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> off_diagonal,
    DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYTRF_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<double> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double real SYTRF_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status SytrfRk(const ReferenceLapackProvider& provider,
                                       DenseBlasTriangle triangle,
                                       DenseBlasMatrixView<double> matrix,
                                       DenseBlasVectorView<double> off_diagonal,
                                       DenseBlasVectorView<index_t> pivots,
                                       const LapackWorkspacePlan& plan,
                                       const LapackWorkspace& workspace,
                                       LapackReport& report);

/** @brief Queries single complex SYTRF_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single complex SYTRF_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<float>> matrix,
        DenseBlasVectorView<std::complex<float>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex SYTRF_RK storage without array reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double complex SYTRF_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<double>> matrix,
        DenseBlasVectorView<std::complex<double>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex Hermitian HETRF_RK storage without array
 * reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes single complex Hermitian HETRF_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrfRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<float>> matrix,
        DenseBlasVectorView<std::complex<float>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Hermitian HETRF_RK storage without array
 * reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower triangle and E convention.
 * @param matrix Square input/output descriptor; entries unread here.
 * @param off_diagonal Disjoint contiguous exact-n E output; entries unread.
 * @param pivots Disjoint contiguous exact-n signed ASC pivot output; unread.
 * @return Formula-bound plan or structural error; no native query.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrfRkWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> off_diagonal,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes double complex Hermitian HETRF_RK with separate D storage.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Selected input/output triangle and E convention.
 * @param matrix Square selected input, replaced by D diagonal and unit U/L.
 * @param off_diagonal Disjoint contiguous exact-n E block offdiagonal output.
 * @param pivots Disjoint contiguous exact-n signed one-based paired output.
 * @param plan Unmodified matching query plan, revalidated before mutation.
 * @param workspace Explicit disjoint typed scalar/layout and byte integer work.
 * @param report Required raw INFO, provenance, outcome and output validity.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw partial factors. Provider defects make all outputs unusable;
 * packed A and pivots are withheld, while direct A/E may have changed.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrfRk(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<std::complex<double>> matrix,
        DenseBlasVectorView<std::complex<double>> off_diagonal,
        DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_RK_H_
