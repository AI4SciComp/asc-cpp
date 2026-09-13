#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_H_
/** @file
 * @brief Checked two-stage Aasen factorization and solution drivers.
 *
 * SYSV_AA_2STAGE/HESV_AA_2STAGE factor the selected symmetric/Hermitian
 * triangle of A, store shifted unit triangular factors in A and band LU in
 * caller-owned contiguous TB, and solve A*X=B. Complex SY uses transpose;
 * HE uses adjoints and ignores original diagonal imaginary components.
 * TB retains its exact supplied LTB; active N requires LTB>=4*N. The block
 * width is min(192,(floor(LTB/N)-1)/3,floor(LWORK/N)), written to TB[0].
 * Exact-N outer and band pivot outputs stay paired with these factors and
 * their original provider/scalar/triangle/symmetry/capacities. They can be
 * reused by the matching two-stage consumers. Existing factor factories
 * and pivot-family meanings remain unchanged.
 *
 * Queries read metadata only. N=0 is a checked noncall preserving all
 * arrays and scratch, with absent native INFO. N>0 still factors when
 * NRHS=0. Caller workspace supplies live scalar WORK, minimum N and the
 * checked rounded 192*N preference; two placement-constructed native
 * INTEGER arrays; and live scalars for row/Hermitian A and row B packing.
 * The provider receives the original LTB and at most preferred LWORK.
 * Plans bind all shapes, capacities, original/effective strides, layouts,
 * options and provider identity. Disjoint accessible operands, workspace
 * and live metadata are required. Source bounds include the driver's
 * unconditional dual query and its native INTEGER 577*N computation.
 *
 * INFO starts at the full native INTEGER minimum. Valid results require
 * INFO in 0..N, source-derived TB block width, both pivot sequences and
 * the final optimal WORK[0] witness. Positive INFO also requires the exact
 * zero band-U diagonal witness. It reports kNumerical/kSingular, a zero-based
 * diagnostic index and kDocumentedPartial factors, with unchanged B. Only
 * INFO=0 publishes the solution. Native INFO=0 is not an accuracy guarantee.
 * Malformed output is a provider defect: packed outputs are withheld;
 * direct A, TB or B may have changed. Structural rejection preserves
 * arrays/scratch; unsafe metadata aliases also preserve the report.
 * Otherwise the report resets before validation. No finiteness scan,
 * allocation, transfer, fallback or global-state change occurs. Disjoint
 * calls are reentrant; callers synchronize shared mutable operands.
 * Required provider range and TB-query limitations remain in the review.
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
/** @brief Metadata-only caller-workspace query for SSYSV_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySysvAa2StageWorkspace(const ReferenceLapackProvider& provider,
                           DenseBlasTriangle triangle,
                           DenseBlasMatrixView<float> matrix,
                           DenseBlasVectorView<float> band,
                           DenseBlasVectorView<index_t> pivots,
                           DenseBlasVectorView<index_t> band_pivots,
                           DenseBlasMatrixView<float> rhs);
/** @brief Executes the exact native SSYSV_AA_2STAGE driver.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned live WORK/packing scalars and INTEGER bytes.
 * @param report Receives provenance, raw INFO, diagnostics and output validity.
 * @return Success, structural rejection, singular partial factors or provider
 * defect.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SysvAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, DenseBlasMatrixView<float> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-workspace query for DSYSV_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySysvAa2StageWorkspace(const ReferenceLapackProvider& provider,
                           DenseBlasTriangle triangle,
                           DenseBlasMatrixView<double> matrix,
                           DenseBlasVectorView<double> band,
                           DenseBlasVectorView<index_t> pivots,
                           DenseBlasVectorView<index_t> band_pivots,
                           DenseBlasMatrixView<double> rhs);
/** @brief Executes the exact native DSYSV_AA_2STAGE driver.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned live WORK/packing scalars and INTEGER bytes.
 * @param report Receives provenance, raw INFO, diagnostics and output validity.
 * @return Success, structural rejection, singular partial factors or provider
 * defect.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SysvAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<double> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, DenseBlasMatrixView<double> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-workspace query for CSYSV_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySysvAa2StageWorkspace(const ReferenceLapackProvider& provider,
                           DenseBlasTriangle triangle,
                           DenseBlasMatrixView<std::complex<float>> matrix,
                           DenseBlasVectorView<std::complex<float>> band,
                           DenseBlasVectorView<index_t> pivots,
                           DenseBlasVectorView<index_t> band_pivots,
                           DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes the exact native CSYSV_AA_2STAGE driver.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned live WORK/packing scalars and INTEGER bytes.
 * @param report Receives provenance, raw INFO, diagnostics and output validity.
 * @return Success, structural rejection, singular partial factors or provider
 * defect.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SysvAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-workspace query for ZSYSV_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySysvAa2StageWorkspace(const ReferenceLapackProvider& provider,
                           DenseBlasTriangle triangle,
                           DenseBlasMatrixView<std::complex<double>> matrix,
                           DenseBlasVectorView<std::complex<double>> band,
                           DenseBlasVectorView<index_t> pivots,
                           DenseBlasVectorView<index_t> band_pivots,
                           DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes the exact native ZSYSV_AA_2STAGE driver.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned live WORK/packing scalars and INTEGER bytes.
 * @param report Receives provenance, raw INFO, diagnostics and output validity.
 * @return Success, structural rejection, singular partial factors or provider
 * defect.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SysvAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-workspace query for CHESV_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryHesvAa2StageWorkspace(const ReferenceLapackProvider& provider,
                           DenseBlasTriangle triangle,
                           DenseBlasMatrixView<std::complex<float>> matrix,
                           DenseBlasVectorView<std::complex<float>> band,
                           DenseBlasVectorView<index_t> pivots,
                           DenseBlasVectorView<index_t> band_pivots,
                           DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes the exact native CHESV_AA_2STAGE driver.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned live WORK/packing scalars and INTEGER bytes.
 * @param report Receives provenance, raw INFO, diagnostics and output validity.
 * @return Success, structural rejection, singular partial factors or provider
 * defect.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status HesvAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-workspace query for ZHESV_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryHesvAa2StageWorkspace(const ReferenceLapackProvider& provider,
                           DenseBlasTriangle triangle,
                           DenseBlasMatrixView<std::complex<double>> matrix,
                           DenseBlasVectorView<std::complex<double>> band,
                           DenseBlasVectorView<index_t> pivots,
                           DenseBlasVectorView<index_t> band_pivots,
                           DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes the exact native ZHESV_AA_2STAGE driver.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Selected original symmetric/Hermitian triangle.
 * @param matrix Original square coefficients overwritten by shifted factors.
 * @param band Contiguous persistent TB output with its exact supplied LTB.
 * @param pivots Exact-N contiguous outer Aasen pivot output.
 * @param band_pivots Exact-N contiguous band-LU pivot output.
 * @param rhs N-by-NRHS right-hand sides, replaced by X only on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned live WORK/packing scalars and INTEGER bytes.
 * @param report Receives provenance, raw INFO, diagnostics and output validity.
 * @return Success, structural rejection, singular partial factors or provider
 * defect.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status HesvAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_DRIVER_H_
