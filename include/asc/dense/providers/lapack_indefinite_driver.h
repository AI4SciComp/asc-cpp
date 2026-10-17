#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_DRIVER_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_DRIVER_H_

/** @file
 * @brief Explicit classic symmetric/Hermitian indefinite direct drivers.
 *
 * SYSV uses A=U*D*U^T or L*D*L^T, including complex symmetric A; HESV
 * uses conjugate transpose for Hermitian A. D contains classic signed paired
 * 1-by-1/2-by-2 blocks. Only selected input/output triangles are referenced.
 * There is no symmetry detection, blanket finiteness scan or algorithm
 * fallback.
 *
 * The actual upstream driver factors A and overwrites B with X. It selects
 * TRS when supplied LWORK<n, and TRS2 otherwise; TRS2 temporarily converts
 * factors and restores the original classic encoding before return. This is
 * not implemented as a wrapper-side substitute sequence of factor and solve.
 * Minimum scalar WORK is one; preferred is the checked pinned 64*n requirement
 * with the routine's actual floating query conversions. Caller capacity is
 * used up to preferred, retaining reduced-work algorithm selection.
 *
 * Active operations require n provider-width kInteger entries for output
 * pivots. Execution begins their trivial lifetimes in caller byte storage;
 * scalar and layout storage already contain live T objects. Row-major A
 * needs n*n explicit layout entries. Original Hermitian A always needs those
 * entries, including column-major, to avoid reads of ignored imaginary
 * diagonals inside the pinned factorization. Only selected offdiagonals and
 * real original diagonals are packed. Row-major B additionally needs n*nrhs
 * entries. Publication touches only selected A and logical B, never padding.
 *
 * Formula queries read metadata, not values, and do not call LAPACK. Plans
 * bind source routine/scalar/triangle, all shapes/layouts, original/effective
 * strides and exact provider build/ABI. Unused one-column strides normalize
 * to row count while the original remains in the key. A nonempty order with
 * nrhs=0 still factors A, using a valid dummy B and effective LDB=max(1,n).
 * n=0 is a successful noncall with no workspace or numerical reads/writes.
 *
 * Operations are synchronous serial-CPU calls of the explicit optional
 * provider. No hidden allocation, transfer, synchronization, provider change
 * or global-state change occurs. All referenced storage and nonempty scratch
 * must be provider-accessible. Numerical operands, scratch and live metadata
 * are disjoint and remain live through execution. Concurrent operations need
 * disjoint writable buffers/reports; no successful-factor lifetime is owned.
 *
 * Structural failures preserve numerical storage and do not enter LAPACK:
 * native INFO is absent and called_provider is false. Unsafe metadata/report
 * aliases leave the report untouched; otherwise it resets before validation.
 * Foreign INFO in [1,n] retains completed selected raw factors and validated
 * raw pivots; B is unchanged. Exact zero identifies singularity; pinned
 * NaN-pivot positive INFO is a distinct partial outcome. Neither certifies
 * a successful factor or solution. INFO=0 retains restored classic factors
 * and X, without an added finite/conditioning guarantee. Unexpected signed
 * INFO, malformed native pivots or inconsistent returned WORK are provider
 * defects with unusable output. Packed outputs/public pivots are withheld
 * on such defects; a direct column-major provider buffer may have changed.
 * Reports retain the actual driver name, never a fabricated TRF origin.
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

/** @brief Queries single real SYSV without array reads or a foreign call.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<float> rhs);

/** @brief Executes actual single real SYSV factor-and-solve.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with restored classic block factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sysv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
     const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYSV without array reads or a foreign call.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<double> rhs);

/** @brief Executes actual double real SYSV factor-and-solve.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with restored classic block factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sysv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
     const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex SYSV with transpose-based symmetry.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Executes actual single complex symmetric SYSV.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with restored transpose-based factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sysv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<std::complex<float>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries double complex SYSV with transpose-based symmetry.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower complex symmetric triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Executes actual double complex symmetric SYSV.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square A, overwritten with restored transpose-based factors.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/layout caller storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sysv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<std::complex<double>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries single complex HESV with Hermitian selected-component input.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Executes actual single complex Hermitian HESV.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square A; original imaginary diagonals ignored, factors output.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/selected-packing caller
 * storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hesv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<std::complex<float>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<float>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @brief Queries double complex HESV with Hermitian selected-component input.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian triangle.
 * @param matrix Square input/output A descriptor; values unread.
 * @param pivots Contiguous exact-n signed ASC pivot output; values unread.
 * @param rhs Independent-layout n-by-nrhs input/output B; values unread.
 * @return Metadata-bound minimum/preferred plan; no foreign call or writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Executes actual double complex Hermitian HESV.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square A; original imaginary diagonals ignored, factors output.
 * @param pivots Contiguous n-entry raw signed paired one-based output.
 * @param rhs Independent-layout B, overwritten with X only on INFO=0.
 * @param plan Unmodified matching formula plan.
 * @param workspace Explicit live scalar/integer/selected-packing caller
 * storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure or provider defect; partial data
 * survive.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hesv(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
     DenseBlasMatrixView<std::complex<double>> matrix,
     DenseBlasVectorView<index_t> pivots,
     DenseBlasMatrixView<std::complex<double>> rhs,
     const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
     LapackReport& report);

/** @name Classic indefinite expert drivers
 * @{
 * SYSVX/HESVX execute the pinned source's condition estimation, solve and
 * iterative refinement. New-factor overloads select FACT=N; Factored
 * overloads select FACT=F and require caller-proven matching A/AF/raw pivots.
 * A and B are immutable; old X, FERR/BERR and (for FACT=N) AF/pivots are
 * unread. No originating TRF report is fabricated; this header does not extend
 * the frozen factor-view factory's accepted origins.
 *
 * Only selected A/AF triangles are referenced. Complex SY is symmetric under
 * transpose, not conjugate transpose. Original HE A ignores imaginary
 * diagonals; actual AF block coefficients are retained in full. All layouts
 * are independent. Every row-major A/AF/B/X uses its logical entry count in
 * caller kLayoutConversion storage, ordered A, AF, B, X. HE FACT=N additionally
 * packs original A even when column-major, avoiding LACPY reads of ignored
 * imaginary diagonals. FACT=F never modifies AF or its input pivots.
 *
 * Active scalar WORK minimum is 3*n real or 2*n complex; FACT=N preferred is
 * max(minimum,64*n), and FACT=F preferred is the checked floating
 * representation of the minimum. Actual capacities select source workspace
 * branches and are capped at the checked preferred capacity. Real kInteger is
 * 2*n entries, disjoint native IPIV [0,n) and IWORK [n,2*n). Complex kInteger
 * is n pivots and kReal is n real entries. Source INTEGER arithmetic and
 * floating query conversions are checked separately from ASC-sized packing.
 *
 * Formula queries are metadata-only and bind every shape, original/effective
 * stride, layout, vector length/increment, triangle, FACT and provider
 * identity. FACT=F signed paired pivots and evaluated zero block divisors are
 * checked before any write or call; zero divisors return native-INFO-absent
 * singular diagnostics. Structural failures preserve numerical buffers. Unsafe
 * aliases to live metadata leave even the mandatory report unchanged; otherwise
 * reports reset to called_provider=false with absent native INFO before
 * preflight.
 *
 * n=0 is a no-call success: RCOND=1, FERR/BERR=0, and no scratch is required.
 * n>0,nrhs=0 still factors/estimates condition, using valid local RHS dummies.
 * FACT=N INFO in [1,n] publishes only partial AF/raw pivots and RCOND=0,
 * leaving X/FERR/BERR unchanged. INFO=n+1 retains complete factor encoding,
 * computed X/error diagnostics and RCOND as an accuracy warning, not a
 * successful factor certificate. INFO=0 retains those outputs; negative or
 * nonfinite diagnostics distinguish provider defect from accuracy warning.
 * Unexpected INFO/pivots/query output is provider-invalid: packed outputs are
 * withheld, but direct foreign destinations and diagnostic scalars may have
 * changed. No general finiteness guarantee or forward-error bound is invented.
 *
 * No hidden allocation, transfer, fallback, global registration or implicit
 * synchronization occurs. All nonempty scratch and referenced operands must
 * be accessible to the explicit serial CPU provider context. All numerical
 * buffers, scratch, provider, plan, workspace and report metadata are disjoint
 * and live through the call. Independent calls require independent outputs.
 */

/** @brief Queries single real symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single real symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sysvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<float> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single real symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single real symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const float> original,
    DenseBlasMatrixView<const float> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const float> rhs, DenseBlasMatrixView<float> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double real symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sysvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<double> factors, DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double real symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const double> original,
    DenseBlasMatrixView<const double> factors, RawLapackPivotView pivots,
    DenseBlasMatrixView<const double> rhs, DenseBlasMatrixView<double> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single complex symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sysvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single complex symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double complex symmetric SYSVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Sysvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySysvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double complex symmetric SYSVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status SysvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex Hermitian HESVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single complex Hermitian HESVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Hesvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex Hermitian HESVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    const float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error);

/** @brief Executes single complex Hermitian HESVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status HesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<float>> original,
    DenseBlasMatrixView<const std::complex<float>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<float>> rhs,
    DenseBlasMatrixView<std::complex<float>> solution,
    float& reciprocal_condition, DenseBlasVectorView<float> forward_error,
    DenseBlasVectorView<float> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Hermitian HESVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvxWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double complex Hermitian HESVX FACT=N.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Square selected AF output; previous entries are unread.
 * @param pivots Contiguous exact-n signed ASC pivot output, initially unread.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status Hesvx(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex Hermitian HESVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; value unread and
 * unchanged.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @return Complete formula plan or structural failure; no writes or calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHesvxFactoredWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    const double& reciprocal_condition,
    DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error);

/** @brief Executes double complex Hermitian HESVX FACT=F.
 * @param provider Explicit checked same-build reference selection.
 * @param triangle Selected upper/lower triangle of A and AF.
 * @param original Immutable original square A, with source symmetry semantics.
 * @param factors Immutable same-provider matching classic AF.
 * @param pivots Immutable raw classic signed paired pivots, exact n.
 * @param rhs Immutable n-by-nrhs original B.
 * @param solution Independent-layout n-by-nrhs X output, initially unread.
 * @param reciprocal_condition RCOND output identity; diagnostic retained.
 * @param forward_error Contiguous nrhs-entry FERR output; initially unread.
 * @param backward_error Contiguous nrhs-entry BERR output; initially unread.
 * @param plan Unmodified matching metadata-bound plan.
 * @param workspace Explicit disjoint live scalar/real/integer/layout storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural/numerical failure, warning or provider defect.
 */
ASC_DENSE_LAPACK_EXPORT Status HesvxFactored(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<const std::complex<double>> original,
    DenseBlasMatrixView<const std::complex<double>> factors,
    RawLapackPivotView pivots,
    DenseBlasMatrixView<const std::complex<double>> rhs,
    DenseBlasMatrixView<std::complex<double>> solution,
    double& reciprocal_condition, DenseBlasVectorView<double> forward_error,
    DenseBlasVectorView<double> backward_error, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @} */

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_DRIVER_H_
