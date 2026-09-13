#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_H_
/** @file
 * @brief Checked solves from immutable two-stage Aasen A/TB and both pivots.
 *
 * A, TB, outer pivots, band pivots, exact original LTB, triangle, scalar and
 * symmetry must originate together from SYTRF_AA_2STAGE/HETRF_AA_2STAGE with
 * the same provider. Common numerical origin is a caller precondition.
 * The shifted unit triangular A and band LU in TB are not interchangeable
 * with single-stage, classic, ROOK or RK factors. TB[0] is the exact real
 * block width NB in 1..min(192,(floor(LTB/N)-1)/3). TB has contiguous LTB
 * scalar entries, at least 4*N for nonempty N; its band-U diagonal row is
 * 2*NB and leading dimension is floor(LTB/N). Outer exact-N kAasen pivots
 * start with min(N,NB) identities, then i+1<=P[i]<=N. Separate exact-N band
 * pivots satisfy i+1<=Q[i]<=min(N,i+NB+1), retaining interleaved LU swaps.
 * Existing factor-view factories and public pivot-family meanings remain
 * unchanged. Complex SY uses transpose; HE uses adjoints. Raw selected
 * factor coefficients, including Hermitian diagonals, remain unchanged.
 *
 * Queries inspect metadata only, with no native query or array reads.
 * Active calls need no scalar WORK; they use 2*N placement-constructed
 * native INTEGER entries in caller byte storage and live scalar packing
 * for row-major A and B. TB stays in its original band storage. Plans bind
 * shape, exact LTB, original/effective strides, layouts/options and provider.
 * N=0 or NRHS=0 is a validated noncall with no scratch or numerical reads.
 * All operands, scratch and live metadata must be accessible and disjoint.
 * No allocation, fallback, implicit transfer or global-state change occurs.
 *
 * Active preflight validates NB, both pivot sequences and exact zero band-U
 * diagonals before scratch/output mutation. A zero diagonal returns
 * kNumerical/kSingular and a zero-based diagnostic index, without native
 * entry or synthetic INFO. This is not a blanket finiteness scan. Native
 * INFO starts at the full-width INTEGER minimum; only zero is valid.
 * Malformed INFO or changed private input pivots are provider defects.
 * Such failure withholds packed B; direct B may change. A, TB and public
 * pivots remain immutable. Structural rejection preserves operands/scratch;
 * unsafe metadata/report aliases also preserve the report. Otherwise the
 * report resets before preflight. INFO zero is not an accuracy certificate.
 * Concurrent calls may share immutable factors/provider/plans with private
 * writable B, workspace and reports. Required provider range limitations
 * remain in the programme review.
 */
#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
namespace asc {
/** @brief Metadata-only caller-storage query for SSYTRS_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrsAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<const float> factors,
                            DenseBlasVectorView<const float> band,
                            RawLapackPivotView pivots,
                            DenseBlasVectorView<const index_t> band_pivots,
                            DenseBlasMatrixView<float> rhs);
/** @brief Executes the exact native SSYTRS_AA_2STAGE factor consumer.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned native INTEGER bytes and live layout scalars.
 * @param report Receives native INFO, provenance, diagnostic and output
 * validity.
 * @return Success, structural rejection, singular preflight or provider defect.
 * @pre All factor buffers have the documented common numerical origin.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrsAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors,
    DenseBlasVectorView<const float> band, RawLapackPivotView pivots,
    DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Metadata-only caller-storage query for DSYTRS_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrsAa2StageWorkspace(const ReferenceLapackProvider& provider,
                            DenseBlasTriangle triangle,
                            DenseBlasMatrixView<const double> factors,
                            DenseBlasVectorView<const double> band,
                            RawLapackPivotView pivots,
                            DenseBlasVectorView<const index_t> band_pivots,
                            DenseBlasMatrixView<double> rhs);
/** @brief Executes the exact native DSYTRS_AA_2STAGE factor consumer.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned native INTEGER bytes and live layout scalars.
 * @param report Receives native INFO, provenance, diagnostic and output
 * validity.
 * @return Success, structural rejection, singular preflight or provider defect.
 * @pre All factor buffers have the documented common numerical origin.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrsAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors,
    DenseBlasVectorView<const double> band, RawLapackPivotView pivots,
    DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Metadata-only caller-storage query for CSYTRS_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes the exact native CSYTRS_AA_2STAGE factor consumer.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned native INTEGER bytes and live layout scalars.
 * @param report Receives native INFO, provenance, diagnostic and output
 * validity.
 * @return Success, structural rejection, singular preflight or provider defect.
 * @pre All factor buffers have the documented common numerical origin.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrsAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-storage query for ZSYTRS_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QuerySytrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes the exact native ZSYTRS_AA_2STAGE factor consumer.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage symmetric factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned native INTEGER bytes and live layout scalars.
 * @param report Receives native INFO, provenance, diagnostic and output
 * validity.
 * @return Success, structural rejection, singular preflight or provider defect.
 * @pre All factor buffers have the documented common numerical origin.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status SytrsAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-storage query for CHETRS_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage Hermitian factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryHetrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes the exact native CHETRS_AA_2STAGE factor consumer.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage Hermitian factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned native INTEGER bytes and live layout scalars.
 * @param report Receives native INFO, provenance, diagnostic and output
 * validity.
 * @return Success, structural rejection, singular preflight or provider defect.
 * @pre All factor buffers have the documented common numerical origin.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status HetrsAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    DenseBlasVectorView<const std::complex<float>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<float>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Metadata-only caller-storage query for ZHETRS_AA_2STAGE.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage Hermitian factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @return Matching checked plan or structural/placement/alias/overflow error.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan>
QueryHetrsAa2StageWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes the exact native ZHETRS_AA_2STAGE factor consumer.
 * @param provider Explicit same-build admitted Reference CPU provider.
 * @param triangle Original selected two-stage Hermitian factor triangle.
 * @param factors Immutable square shifted unit triangular A factor.
 * @param band Immutable contiguous TB with its exact original LTB.
 * @param pivots Immutable exact-N outer kAasen raw pivots from the same call.
 * @param band_pivots Immutable exact-N contiguous band-LU pivot sequence.
 * @param rhs Mutable N-by-NRHS right-hand sides, replaced by X on success.
 * @param plan Unmodified matching query plan, revalidated before execution.
 * @param workspace Caller-owned native INTEGER bytes and live layout scalars.
 * @param report Receives native INFO, provenance, diagnostic and output
 * validity.
 * @return Success, structural rejection, singular preflight or provider defect.
 * @pre All factor buffers have the documented common numerical origin.
 */
[[nodiscard]] ASC_DENSE_LAPACK_EXPORT Status HetrsAa2Stage(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    DenseBlasVectorView<const std::complex<double>> band,
    RawLapackPivotView pivots, DenseBlasVectorView<const index_t> band_pivots,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_TWO_STAGE_SOLVE_H_
