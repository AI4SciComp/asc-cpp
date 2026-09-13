#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_H_

/** @file
 * @brief Checked Reference two-stage Aasen symmetric/Hermitian producers.
 *
 * TB is caller-owned persistent band-factor storage, in scalar entries.
 * For N>0 its contiguous length LTB must be at least 4*N; 577*N entries
 * permit the pinned preferred block width 192 when scalar scratch also has
 * at least 192*N entries. Actual NB=min(192,(floor(LTB/N)-1)/3,LWORK/N).
 * TB[0] records NB; subsequent band-LU storage uses leading dimension
 * floor(LTB/N), diagonal row 2*NB, and lower/upper input bandwidths NB.
 * TB, A, both pivot arrays, triangle, symmetry and the exact original LTB
 * must be retained together for a matching two-stage consumer. The A factor
 * is shifted by NB, unlike the single-stage Aasen representation. The first
 * min(N,NB) outer pivots are identities; later entries are sequential
 * one-based symmetric swaps. Band pivots are separate interleaved LU swaps.
 *
 * Queries read metadata only, without allocation or native entry. Execution
 * uses caller-owned live scalar scratch and placement-constructed private
 * native INTEGER arrays in integer workspace. Row-major A and all Hermitian
 * A are explicitly packed; original Hermitian imaginary diagonals are ignored.
 * Scalar minimum is N, preferred is the safely rounded pinned 192*N value.
 * Plans bind shape, layouts, strides, LTB, options and provider/scalar
 * identity. N=0 is a validated noncall, requiring no scratch and leaving arrays
 * unchanged. The supplied A, TB, pivots, workspace and metadata must be
 * disjoint and accessible to the selected CPU provider. TB and pivot increments
 * must be one.
 *
 * INFO>0 means singular band LU: factors and both pivot arrays are published
 * with kNumerical/kSingular and kDocumentedPartial, with a zero-based
 * diagnostic column. Such factors cannot establish a usable inverse or
 * solution. Malformed native INFO, NB, pivots or singular diagonal witness are
 * provider defects; public pivots and packed A are withheld, while direct A and
 * TB may have changed. TB padding within LTB is unspecified. Unselected A and A
 * padding are preserved. The report names the exact native routine and retains
 * kAasen family metadata; it does not make single-stage and two-stage factors
 * interchangeable. INFO zero alone is not a numerical accuracy certificate.
 * Required provider numerical and workspace limitations remain documented in
 * the programme review.
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
/** @brief Metadata-only workspace query for SSYTRF_AA_2STAGE.
 * @param provider Explicit admitted Reference CPU provider.
 * @param triangle Selected triangle of the original symmetric matrix.
 * @param matrix Mutable square original A; transpose symmetry, including
 * complex scalars.
 * @param band Contiguous persistent TB output, at least 4*N scalar entries for
 * active N.
 * @param pivots Contiguous exact-N output of outer symmetric interchanges.
 * @param band_pivots Contiguous exact-N output of band-LU interchanges.
 * @return Bound caller-workspace plan, or structural/placement/alias/overflow
 * error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrfAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<float> matrix,
                            DenseBlasVectorView<float> band,
                            DenseBlasVectorView<index_t> pivots,
                            DenseBlasVectorView<index_t> band_pivots);

/** @brief Factor A through the exact SSYTRF_AA_2STAGE native producer.
 * @param provider Provider bound by the plan.
 * @param triangle Original selected triangle, retained with the produced
 * factors.
 * @param matrix Original A, replaced by the shifted two-stage triangular
 * factor.
 * @param band Persistent TB output; retain its exact length with all factors.
 * @param pivots Exact-N one-based outer pivot output, published after
 * validation.
 * @param band_pivots Exact-N one-based band-LU pivot output, published after
 * validation.
 * @param plan Matching result of QuerySytrfAa2StageWorkspace.
 * @param workspace Caller-owned live scalar, private native INTEGER and packing
 * storage.
 * @param report Receives native INFO, originating routine, outcome and output
 * validity.
 * @return Success, structural error, provider defect, or singular documented
 * partial factors.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrfAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Metadata-only workspace query for DSYTRF_AA_2STAGE.
 * @param provider Explicit admitted Reference CPU provider.
 * @param triangle Selected triangle of the original symmetric matrix.
 * @param matrix Mutable square original A; transpose symmetry, including
 * complex scalars.
 * @param band Contiguous persistent TB output, at least 4*N scalar entries for
 * active N.
 * @param pivots Contiguous exact-N output of outer symmetric interchanges.
 * @param band_pivots Contiguous exact-N output of band-LU interchanges.
 * @return Bound caller-workspace plan, or structural/placement/alias/overflow
 * error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrfAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<double> matrix,
                            DenseBlasVectorView<double> band,
                            DenseBlasVectorView<index_t> pivots,
                            DenseBlasVectorView<index_t> band_pivots);

/** @brief Factor A through the exact DSYTRF_AA_2STAGE native producer.
 * @param provider Provider bound by the plan.
 * @param triangle Original selected triangle, retained with the produced
 * factors.
 * @param matrix Original A, replaced by the shifted two-stage triangular
 * factor.
 * @param band Persistent TB output; retain its exact length with all factors.
 * @param pivots Exact-N one-based outer pivot output, published after
 * validation.
 * @param band_pivots Exact-N one-based band-LU pivot output, published after
 * validation.
 * @param plan Matching result of QuerySytrfAa2StageWorkspace.
 * @param workspace Caller-owned live scalar, private native INTEGER and packing
 * storage.
 * @param report Receives native INFO, originating routine, outcome and output
 * validity.
 * @return Success, structural error, provider defect, or singular documented
 * partial factors.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrfAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<double> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Metadata-only workspace query for CSYTRF_AA_2STAGE.
 * @param provider Explicit admitted Reference CPU provider.
 * @param triangle Selected triangle of the original symmetric matrix.
 * @param matrix Mutable square original A; transpose symmetry, including
 * complex scalars.
 * @param band Contiguous persistent TB output, at least 4*N scalar entries for
 * active N.
 * @param pivots Contiguous exact-N output of outer symmetric interchanges.
 * @param band_pivots Contiguous exact-N output of band-LU interchanges.
 * @return Bound caller-workspace plan, or structural/placement/alias/overflow
 * error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrfAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<std::complex<float>> matrix,
                            DenseBlasVectorView<std::complex<float>> band,
                            DenseBlasVectorView<index_t> pivots,
                            DenseBlasVectorView<index_t> band_pivots);

/** @brief Factor A through the exact CSYTRF_AA_2STAGE native producer.
 * @param provider Provider bound by the plan.
 * @param triangle Original selected triangle, retained with the produced
 * factors.
 * @param matrix Original A, replaced by the shifted two-stage triangular
 * factor.
 * @param band Persistent TB output; retain its exact length with all factors.
 * @param pivots Exact-N one-based outer pivot output, published after
 * validation.
 * @param band_pivots Exact-N one-based band-LU pivot output, published after
 * validation.
 * @param plan Matching result of QuerySytrfAa2StageWorkspace.
 * @param workspace Caller-owned live scalar, private native INTEGER and packing
 * storage.
 * @param report Receives native INFO, originating routine, outcome and output
 * validity.
 * @return Success, structural error, provider defect, or singular documented
 * partial factors.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrfAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Metadata-only workspace query for ZSYTRF_AA_2STAGE.
 * @param provider Explicit admitted Reference CPU provider.
 * @param triangle Selected triangle of the original symmetric matrix.
 * @param matrix Mutable square original A; transpose symmetry, including
 * complex scalars.
 * @param band Contiguous persistent TB output, at least 4*N scalar entries for
 * active N.
 * @param pivots Contiguous exact-N output of outer symmetric interchanges.
 * @param band_pivots Contiguous exact-N output of band-LU interchanges.
 * @return Bound caller-workspace plan, or structural/placement/alias/overflow
 * error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrfAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<std::complex<double>> matrix,
                            DenseBlasVectorView<std::complex<double>> band,
                            DenseBlasVectorView<index_t> pivots,
                            DenseBlasVectorView<index_t> band_pivots);

/** @brief Factor A through the exact ZSYTRF_AA_2STAGE native producer.
 * @param provider Provider bound by the plan.
 * @param triangle Original selected triangle, retained with the produced
 * factors.
 * @param matrix Original A, replaced by the shifted two-stage triangular
 * factor.
 * @param band Persistent TB output; retain its exact length with all factors.
 * @param pivots Exact-N one-based outer pivot output, published after
 * validation.
 * @param band_pivots Exact-N one-based band-LU pivot output, published after
 * validation.
 * @param plan Matching result of QuerySytrfAa2StageWorkspace.
 * @param workspace Caller-owned live scalar, private native INTEGER and packing
 * storage.
 * @param report Receives native INFO, originating routine, outcome and output
 * validity.
 * @return Success, structural error, provider defect, or singular documented
 * partial factors.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrfAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Metadata-only workspace query for CHETRF_AA_2STAGE.
 * @param provider Explicit admitted Reference CPU provider.
 * @param triangle Selected triangle of the original Hermitian matrix.
 * @param matrix Mutable square original A; adjoint symmetry and ignored
 * imaginary diagonals.
 * @param band Contiguous persistent TB output, at least 4*N scalar entries for
 * active N.
 * @param pivots Contiguous exact-N output of outer symmetric interchanges.
 * @param band_pivots Contiguous exact-N output of band-LU interchanges.
 * @return Bound caller-workspace plan, or structural/placement/alias/overflow
 * error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryHetrfAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<std::complex<float>> matrix,
                            DenseBlasVectorView<std::complex<float>> band,
                            DenseBlasVectorView<index_t> pivots,
                            DenseBlasVectorView<index_t> band_pivots);

/** @brief Factor A through the exact CHETRF_AA_2STAGE native producer.
 * @param provider Provider bound by the plan.
 * @param triangle Original selected triangle, retained with the produced
 * factors.
 * @param matrix Original A, replaced by the shifted two-stage triangular
 * factor.
 * @param band Persistent TB output; retain its exact length with all factors.
 * @param pivots Exact-N one-based outer pivot output, published after
 * validation.
 * @param band_pivots Exact-N one-based band-LU pivot output, published after
 * validation.
 * @param plan Matching result of QueryHetrfAa2StageWorkspace.
 * @param workspace Caller-owned live scalar, private native INTEGER and packing
 * storage.
 * @param report Receives native INFO, originating routine, outcome and output
 * validity.
 * @return Success, structural error, provider defect, or singular documented
 * partial factors.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status HetrfAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Metadata-only workspace query for ZHETRF_AA_2STAGE.
 * @param provider Explicit admitted Reference CPU provider.
 * @param triangle Selected triangle of the original Hermitian matrix.
 * @param matrix Mutable square original A; adjoint symmetry and ignored
 * imaginary diagonals.
 * @param band Contiguous persistent TB output, at least 4*N scalar entries for
 * active N.
 * @param pivots Contiguous exact-N output of outer symmetric interchanges.
 * @param band_pivots Contiguous exact-N output of band-LU interchanges.
 * @return Bound caller-workspace plan, or structural/placement/alias/overflow
 * error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryHetrfAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<std::complex<double>> matrix,
                            DenseBlasVectorView<std::complex<double>> band,
                            DenseBlasVectorView<index_t> pivots,
                            DenseBlasVectorView<index_t> band_pivots);

/** @brief Factor A through the exact ZHETRF_AA_2STAGE native producer.
 * @param provider Provider bound by the plan.
 * @param triangle Original selected triangle, retained with the produced
 * factors.
 * @param matrix Original A, replaced by the shifted two-stage triangular
 * factor.
 * @param band Persistent TB output; retain its exact length with all factors.
 * @param pivots Exact-N one-based outer pivot output, published after
 * validation.
 * @param band_pivots Exact-N one-based band-LU pivot output, published after
 * validation.
 * @param plan Matching result of QueryHetrfAa2StageWorkspace.
 * @param workspace Caller-owned live scalar, private native INTEGER and packing
 * storage.
 * @param report Receives native INFO, originating routine, outcome and output
 * validity.
 * @return Success, structural error, provider defect, or singular documented
 * partial factors.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status HetrfAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> band,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasVectorView<index_t> band_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_H_
