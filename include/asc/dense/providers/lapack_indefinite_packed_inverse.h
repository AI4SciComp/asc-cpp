#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_INVERSE_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_INVERSE_H_

/** @file
 * @brief Explicit in-place packed symmetric and Hermitian inversion.
 *
 * SPTRI uses ordinary transpose, including complex symmetric factors; HPTRI
 * uses adjoint. Mutable packed factors and immutable exact-N kBunchKaufman
 * signed paired pivots retain same-call SPTRF/HPTRF scalar/provider/triangle/
 * symmetry provenance. Completed singular factors are accepted for actual
 * native INFO. Raw descriptors cannot prove that history or numerical content;
 * existing dense, Rook, RK and Aasen factor factories gain no packed meaning.
 * Overwriting factors invalidates borrowed factor views of that storage.
 *
 * Metadata-only queries bind routine/scalar/provider ABI, order, triangle,
 * symmetry, packed layout and exact pivot count. Nonempty execution requires
 * N live T scalar WORK entries and N placement-constructed native INTEGER
 * objects in caller byte storage. Row-major factors additionally require
 * N*(N+1)/2 live T packing entries. Both triangles check the full native
 * N*(N+1) product, packed loop/BLAS terminal cursors and byte totals before
 * array access. No native workspace query or WORK result channel exists.
 * Empty order is a successful noncall without numerical reads, scratch or INFO.
 *
 * All operands, scratch and live provider/plan/workspace/report metadata are
 * disjoint and accessible to the explicit serial CPU context. Structural
 * rejection preserves numerical arrays and scratch; unsafe metadata aliases
 * also preserve the report. Otherwise the report resets before further
 * validation. Every paired pivot and directional swap bound is checked before
 * conversion. There is no allocation, transfer, global handler change,
 * synchronization, fallback or substitute numerical algorithm. Concurrent calls
 * may share immutable provider/plans/pivots with disjoint mutable AP, scratch
 * and reports.
 *
 * Factor packing preserves all raw coefficients. HPTRI's initial singular
 * scan compares the complete complex diagonal to zero; its subsequent inverse
 * arithmetic uses real diagonals. No original-input Hermitian normalization is
 * applied. The source scans exactly zero positive-pivot 1x1 D entries upper
 * N-to-1 or lower 1-to-N before overwriting AP. Positive INFO reports that
 * one-based index, kSingular/kDocumentedPartial and its zero-based diagnostic
 * index. Factors are then unchanged and no inverse exists. The source does not
 * diagnose singular 2x2 blocks or guarantee finite/conditioned inverse results.
 *
 * Full-width INFO starts at the provider INTEGER minimum. Missing, partial-
 * width, negative or source-inconsistent INFO, or altered native input pivots,
 * is a provider defect with raw INFO and unusable output. Row-major AP
 * publication is withheld; direct column-major AP may retain native effects.
 * INFO=0 publishes the selected packed inverse. Public pivots and storage
 * outside the selected packed span stay unchanged. No full matrix is formed,
 * no factor-family certificate is issued, and WORK has no specified result.
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

/** @brief Queries single real SSPTRI workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable packed raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kBunchKaufman pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<float> factors, RawLapackPivotView pivots);

/** @brief Executes native single real SSPTRI in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor classic block pivots; never
 * overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status Sptri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<float> factors,
                                     RawLapackPivotView pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real DSPTRI workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable packed raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kBunchKaufman pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<double> factors, RawLapackPivotView pivots);

/** @brief Executes native double real DSPTRI in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor classic block pivots; never
 * overwritten.
 * @param plan Unmodified matching formula query plan.
 * @param workspace Caller-owned disjoint live scalar/layout and INTEGER
 * storage.
 * @param report Mandatory native INFO, provenance and output-validity report.
 * @return OK, preflight error, singular numerical result or provider defect.
 * @pre Factors and pivots have the documented same-provider origin; overwriting
 * invalidates any borrowed factor views. INFO=0 is not a finite-inverse
 * promise.
 */
ASC_DENSE_LAPACK_EXPORT Status Sptri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasPackedMatrixView<double> factors,
                                     RawLapackPivotView pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single complex CSPTRI workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable packed raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kBunchKaufman pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single complex CSPTRI in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor classic block pivots; never
 * overwritten.
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
Sptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<float>> factors,
      RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex ZSPTRI workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable packed raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kBunchKaufman pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double complex ZSPTRI in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor classic block pivots; never
 * overwritten.
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
Sptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<double>> factors,
      RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex CHPTRI workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable packed raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kBunchKaufman pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<float>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native single complex CHPTRI in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor classic block pivots; never
 * overwritten.
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
Hptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<float>> factors,
      RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex ZHPTRI workspace without array reads.
 * @param provider Explicit checked provider; unchanged.
 * @param triangle Selected upper/lower factor and inverse triangle.
 * @param factors Mutable packed raw factor descriptor; values unread here.
 * @param pivots Same-operation immutable signed kBunchKaufman pivots, exact n.
 * @return Formula plan or structural/placement/alias/arithmetic failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasPackedMatrixView<std::complex<double>> factors,
    RawLapackPivotView pivots);

/** @brief Executes native double complex ZHPTRI in place.
 * @param provider Explicit same-build provider; no fallback.
 * @param triangle Same-call selected upper/lower triangle.
 * @param factors Raw factor, replaced by selected inverse on native success.
 * @param pivots Immutable same-factor classic block pivots; never
 * overwritten.
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
Hptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasPackedMatrixView<std::complex<double>> factors,
      RawLapackPivotView pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_PACKED_INVERSE_H_
