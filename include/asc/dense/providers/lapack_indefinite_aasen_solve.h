#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_SOLVE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_SOLVE_H_
/** @file
 * @brief Checked solves from immutable single-stage Aasen factors and pivots.
 *
 * Selected square A and exact-n kAasen raw pivots must originate together
 * from the same provider, scalar, triangle and SYTRF_AA/HETRF_AA operation.
 * Common numerical origin is a caller precondition. These are single-stage
 * shifted unit triangular multipliers and tridiagonal T; classic, ROOK, RK
 * and two-stage factor encodings are not accepted by this contract. Pivots
 * are positive one-based symmetric swaps: first entry one, each later target
 * in its own index..n range. SY uses transpose symmetry, including complex
 * symmetric input; HE uses adjoints. Complete raw factor coefficients are
 * retained, including complex Hermitian diagonals. Padding and unselected
 * entries are unchanged. Existing factor-view factories are not expanded.
 *
 * Queries read metadata only and make no native query. Active calls require
 * scalar WORK with minimum 3*n-2 and rounded preferred entries, n private
 * provider INTEGER pivots in caller byte storage with explicit lifetimes,
 * plus n*n live scalar layout entries for row A and n*nrhs for row B.
 * Column storage is direct. Both original and effective strides, source
 * INTEGER arithmetic, packing/byte totals and disjoint accessible storage
 * are checked. Empty n or nrhs is a validated noncall with no numerical reads
 * or workspace, before pivot-value validation. Native execution WORK is
 * scratch, not a returned size recommendation. No allocation or fallback
 * occurs; no global state or floating-point environment changes.
 *
 * Structural rejection preserves arrays and scratch. Unsafe report/metadata
 * aliases also preserve the report; otherwise it resets before preflight.
 * INFO starts at the full-width native INTEGER minimum. Private input pivots
 * and a claimed singular WORK diagonal are checked after native execution.
 * Missing/partial/negative/out-of-range INFO, changed pivots or an inconsistent
 * positive INFO are provider defects. A consistent positive INFO from the
 * internal tridiagonal solve is a numerical singular failure with zero-based
 * diagnostic index and unusable solution output; the native routine still
 * performs its final triangular operation and reverse swaps. Packed B is
 * withheld on failure, while direct B may have changed. A and public pivots
 * remain immutable. INFO zero denotes completion, not a finite or accurate
 * solution certificate. Concurrent calls may share immutable inputs, provider
 * and plans, with distinct writable B, scratch and reports.
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
/** @brief Queries single real Aasen solve storage without array reads.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @return Matching caller-storage plan or structural/placement/alias/overflow
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<float> rhs);
/** @brief Executes native single real Aasen solve with immutable factors.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @param plan Unmodified matching metadata query plan, revalidated on
 * execution.
 * @param workspace Disjoint live scalar WORK, native INTEGER and layout
 * storage.
 * @param report Required INFO, Aasen provenance, diagnostic and output
 * validity.
 * @return OK, structural error, singular numerical error or provider defect.
 * @pre Factors and pivots share the documented provider and operation origin.
 * Failure withholds packed B; direct B may change. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
        DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries double real Aasen solve storage without array reads.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @return Matching caller-storage plan or structural/placement/alias/overflow
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<double> rhs);
/** @brief Executes native double real Aasen solve with immutable factors.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @param plan Unmodified matching metadata query plan, revalidated on
 * execution.
 * @param workspace Disjoint live scalar WORK, native INTEGER and layout
 * storage.
 * @param report Required INFO, Aasen provenance, diagnostic and output
 * validity.
 * @return OK, structural error, singular numerical error or provider defect.
 * @pre Factors and pivots share the documented provider and operation origin.
 * Failure withholds packed B; direct B may change. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
        DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
        const LapackWorkspace& workspace, LapackReport& report);
/** @brief Queries single complex symmetric Aasen solve storage without array
 * reads.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @return Matching caller-storage plan or structural/placement/alias/overflow
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native single complex symmetric Aasen solve with immutable
 * factors.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @param plan Unmodified matching metadata query plan, revalidated on
 * execution.
 * @param workspace Disjoint live scalar WORK, native INTEGER and layout
 * storage.
 * @param report Required INFO, Aasen provenance, diagnostic and output
 * validity.
 * @return OK, structural error, singular numerical error or provider defect.
 * @pre Factors and pivots share the documented provider and operation origin.
 * Failure withholds packed B; direct B may change. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<const std::complex<float>> factors,
        RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
        const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
        LapackReport& report);
/** @brief Queries double complex symmetric Aasen solve storage without array
 * reads.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @return Matching caller-storage plan or structural/placement/alias/overflow
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native double complex symmetric Aasen solve with immutable
 * factors.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @param plan Unmodified matching metadata query plan, revalidated on
 * execution.
 * @param workspace Disjoint live scalar WORK, native INTEGER and layout
 * storage.
 * @param report Required INFO, Aasen provenance, diagnostic and output
 * validity.
 * @return OK, structural error, singular numerical error or provider defect.
 * @pre Factors and pivots share the documented provider and operation origin.
 * Failure withholds packed B; direct B may change. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status SytrsAa(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
/** @brief Queries single complex Hermitian Aasen solve storage without array
 * reads.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @return Matching caller-storage plan or structural/placement/alias/overflow
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Executes native single complex Hermitian Aasen solve with immutable
 * factors.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @param plan Unmodified matching metadata query plan, revalidated on
 * execution.
 * @param workspace Disjoint live scalar WORK, native INTEGER and layout
 * storage.
 * @param report Required INFO, Aasen provenance, diagnostic and output
 * validity.
 * @return OK, structural error, singular numerical error or provider defect.
 * @pre Factors and pivots share the documented provider and operation origin.
 * Failure withholds packed B; direct B may change. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrsAa(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
        DenseBlasMatrixView<const std::complex<float>> factors,
        RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<float>> rhs,
        const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
        LapackReport& report);
/** @brief Queries double complex Hermitian Aasen solve storage without array
 * reads.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @return Matching caller-storage plan or structural/placement/alias/overflow
 * error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrsAaWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Executes native double complex Hermitian Aasen solve with immutable
 * factors.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param triangle Selected upper or lower single-stage Aasen factor triangle.
 * @param factors Immutable square shifted triangular factors and T.
 * @param pivots Immutable exact-n same-operation positive kAasen raw pivots.
 * @param rhs Mutable n-by-nrhs right-hand sides; successful output is X.
 * @param plan Unmodified matching metadata query plan, revalidated on
 * execution.
 * @param workspace Disjoint live scalar WORK, native INTEGER and layout
 * storage.
 * @param report Required INFO, Aasen provenance, diagnostic and output
 * validity.
 * @return OK, structural error, singular numerical error or provider defect.
 * @pre Factors and pivots share the documented provider and operation origin.
 * Failure withholds packed B; direct B may change. See the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status HetrsAa(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots, DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);
}  // namespace asc
#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_AASEN_SOLVE_H_
