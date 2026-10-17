#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_INVERSE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_INVERSE_H_

/** @file
 * @brief Explicit in-place rook symmetric and Hermitian inversion.
 *
 * SYTRI_ROOK inverts transpose-symmetric factors, including complex symmetric
 * factors. HETRI_ROOK inverts conjugate-transpose Hermitian factors. The caller
 * supplies mutable selected square factor storage and immutable raw kRook
 * pivots from one same-provider, same-scalar SYTRF_ROOK/SYTF2_ROOK or
 * HETRF_ROOK/HETF2_ROOK operation (including the corresponding direct driver).
 * Classic equal-negative-pair pivots are not interchangeable with rook pivots.
 * Adjacent negative entries each encode their own ordered interchange.
 * Provenance and numerical content are caller preconditions, not inferred
 * certificates. Completed singular factors are accepted for native INFO.
 * Overwriting factors invalidates all borrowed views of those buffers.
 *
 * Only the selected triangle is overwritten by the inverse. Other entries and
 * padding, and every public pivot entry, remain unchanged. Hermitian factor
 * packing retains complete raw diagonal and block coefficients: the native
 * singular check compares a complete complex diagonal with zero before the
 * inversion arithmetic uses its real part. No original-input normalization is
 * applied to factor storage. No full symmetric/Hermitian output is
 * materialized.
 *
 * Active calls require n live T scalar WORK entries and n provider-width signed
 * INTEGER entries in caller byte storage. Row-major A additionally needs n*n
 * live T layout entries. The formula query inspects metadata only, makes no
 * foreign call, and does not read factor or pivot entries. Plans bind routine,
 * scalar, provider/ABI, triangle, symmetry, order, layout and
 * original/effective leading dimensions. N=0 needs no workspace or numerical
 * reads/writes and succeeds without a foreign call. Source loop and BLAS cursor
 * arithmetic is checked against the selected INTEGER width before numerical
 * access.
 *
 * All numerical operands, scratch and live metadata must be disjoint and
 * accessible to the explicit CPU context. Structural errors preserve numerical
 * buffers and scratch. Metadata/report alias rejection also preserves the
 * report; otherwise the report is reset with no INFO and called_provider=false
 * before subsequent preflight. Independent concurrent calls need disjoint
 * writable buffers/reports. Operations allocate nothing, transfer nothing,
 * change no global state and use no fallback algorithm or provider.
 *
 * Native INFO>0 reports the first exactly zero 1-by-1 D entry in the source's
 * scan order (upper: n to 1; lower: 1 to n). Native A is then unchanged; no
 * inverse exists in the output. The report retains the one-based INFO and a
 * zero-based diagnostic index, with kSingular/kDocumentedPartial. Native
 * INFO=0 records completion, without a finiteness or conditioning guarantee.
 * The source does not diagnose singular 2-by-2 blocks or scale all intermediate
 * inverse arithmetic. Numerical acceptance is separate from native fidelity.
 *
 * Full-width INFO starts at INTEGER minimum after preflight. An unwritten,
 * partial-width, negative or source-inconsistent INFO is a provider defect,
 * as is unexpected mutation of converted input pivots. Raw INFO is retained
 * with unusable output. Packed output is withheld; direct
 * column-major factor storage may already have changed. WORK has no documented
 * returned value and is not a query/result channel. No RCOND/FERR/BERR exist.
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

/** @brief Queries single real SSYTRI_ROOK workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable square raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kRook pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytriRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> factors, RawLapackPivotView pivots);

/** @brief Executes native single real SSYTRI_ROOK in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor rook block pivots; never overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytriRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<float> factors, RawLapackPivotView pivots,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries double real DSYTRI_ROOK workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable square raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kRook pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytriRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> factors, RawLapackPivotView pivots);

/** @brief Executes native double real DSYTRI_ROOK in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor rook block pivots; never overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytriRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<double> factors, RawLapackPivotView pivots,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries single complex CSYTRI_ROOK workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable square raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kRook pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytriRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single complex CSYTRI_ROOK in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor rook block pivots; never overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytriRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<float>> factors,
          RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex ZSYTRI_ROOK workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable square raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kRook pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytriRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double complex ZSYTRI_ROOK in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor rook block pivots; never overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytriRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<double>> factors,
          RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex CHETRI_ROOK workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable square raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kRook pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetriRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single complex CHETRI_ROOK in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor rook block pivots; never overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetriRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<float>> factors,
          RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex ZHETRI_ROOK workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable square raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kRook pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetriRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double complex ZHETRI_ROOK in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor rook block pivots; never overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetriRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<double>> factors,
          RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_INVERSE_H_
