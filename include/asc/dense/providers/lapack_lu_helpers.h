#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_HELPERS_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_HELPERS_H_

/** @file
 * @brief Checked explicit reference row swaps and general-matrix scaling.
 *
 * LASWP applies sequential swaps, not a final permutation. first_row is an ASC
 * zero-based row; row_count rows starting there are selected. The raw one-based
 * pivot for row first_row+j is values[first_row+j*abs(pivot_increment)]. A
 * negative increment reverses the order of those swaps, not their association.
 * Only selected payload slots are read and validated in [1,m]. The LU raw
 * sequential-swap tag is required; GETRF's stronger pivot>=step condition is
 * not imposed. Zero increment, zero selected rows or zero columns is a no-op.
 * A no-op needs no pivot entries or workspace but still validates metadata.
 *
 * Nonempty LASWP converts selected raw entries into explicitly supplied
 * ABI-width kInteger storage. Its capacity reaches the last selected raw slot;
 * unused prefix/gap slots are not read or overwritten. Row-major A additionally
 * uses m*n live scalar objects in kLayoutConversion. Column-major needs no
 * scalar work. Range/stride arithmetic includes native final loop/index
 * updates, not only the last accessed element.
 *
 * LAQGE applies supplied scales using the pinned source's 0.1 ratio threshold
 * and SMALL=safe_minimum/precision, LARGE=1/SMALL. Statistics must be finite,
 * condition ratios in [0,1], and AMAX nonnegative. Selected scales must be
 * finite positive and contiguous with exact m/n lengths. Unused scale vectors
 * may be empty or full length; their values are not read. Statistics are
 * caller-supplied decision data, not recomputed from A or the scale vectors.
 * The caller is responsible for their common provenance. Complex matrices use
 * real scale vectors. Both-scale multiplication preserves the provider's
 * C[j]*R[i]*A[i,j] evaluation, not a reassociated overflow-avoidance algorithm.
 * No finite-output guarantee is added for arbitrary inputs or intermediate
 * overflow. After an actual scaling branch, nonfinite logical output yields
 * kNumerical with kAccuracyWarning and kDocumentedPartial; all raw matrix
 * values and the applied N/R/C/B result are preserved. Finite entries retain
 * their computed values; nonfinite entries have no finite-accuracy claim.
 * The no-scaling branch neither scans nor certifies existing matrix values.
 *
 * Queries are checked formulas, never foreign LWORK queries. Plans bind all
 * metadata, exact provider identity and, for LAQGE, statistic bit patterns.
 * LAQGE row-major plans reserve m*n scalar packing entries; the no-scaling
 * branch leaves A and packing values unread/unmodified. Empty LAQGE does not
 * read statistic/scale values and writes only kNone without a foreign call.
 * Neither upstream routine has INFO:
 * reports keep native_info absent even on actual foreign calls, and do not
 * certify an LU factor. Malformed metadata/workspace fails before all writes.
 * A provider-invalid EQUED leaves applied unchanged and gives unusable output
 * with no invented INFO. Raw column-major A effects remain visible; row-major
 * raw effects remain in caller packing without publishing them into A.
 *
 * All operands and live workspace are pairwise disjoint, host and admitted
 * by the explicit serial context, borrowed for the call. Pinned, device and
 * managed descriptors are not implicit transfer requests and are rejected.
 * Statistics and applied output are disjoint host
 * objects. No allocation, hidden packing, transfer, synchronization, fallback
 * or process-global handler change occurs. Independent calls with disjoint
 * mutable operands/reports are reentrant. LASWP costs O(n*row_count) plus
 * explicit packing; LAQGE costs O(m*n) when scaling. Base Dense remains
 * provider-free; this header belongs only to the optional Dense facet.
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
#include "asc/dense/providers/lapack_lu_driver.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"

namespace asc {

/** @brief Queries single real LASWP conversion and packing capacities.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param first_row Zero-based first selected matrix row, at most m.
 * @param row_count Number of selected consecutive rows, at most m-first_row.
 * @param pivots Unchanged raw one-based sequential swap payload; see file.
 * @param pivot_increment Signed raw-slot spacing and application direction.
 * @return Metadata-bound caller plan or structural error without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    index_t first_row, extent_t row_count, RawLapackPivotView pivots,
    index_t pivot_increment);
/** @brief Applies single real sequential row swaps without allocation.
 * @param provider Explicit reference selection with no fallback.
 * @param matrix Mutable m-by-n matrix; padding is preserved.
 * @param first_row Zero-based first selected row.
 * @param row_count Number of selected consecutive rows.
 * @param pivots Immutable one-based raw selected row destinations.
 * @param pivot_increment Signed slot spacing; zero is an explicit no-op.
 * @param plan Unmodified matching checked formula query result.
 * @param workspace Disjoint caller integer/packing storage; see file contract.
 * @param report Mandatory reset-before-preflight report; INFO stays absent.
 * @return OK or structural error; no numerical singularity test is performed.
 */
ASC_DENSE_LAPACK_EXPORT Status Laswp(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    index_t first_row, extent_t row_count, RawLapackPivotView pivots,
    index_t pivot_increment, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real LASWP conversion and packing capacities.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param first_row Zero-based first selected matrix row, at most m.
 * @param row_count Number of selected consecutive rows, at most m-first_row.
 * @param pivots Unchanged raw one-based sequential swap payload; see file.
 * @param pivot_increment Signed raw-slot spacing and application direction.
 * @return Metadata-bound caller plan or structural error without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    index_t first_row, extent_t row_count, RawLapackPivotView pivots,
    index_t pivot_increment);
/** @brief Applies double real sequential row swaps without allocation.
 * @param provider Explicit reference selection with no fallback.
 * @param matrix Mutable m-by-n matrix; padding is preserved.
 * @param first_row Zero-based first selected row.
 * @param row_count Number of selected consecutive rows.
 * @param pivots Immutable one-based raw selected row destinations.
 * @param pivot_increment Signed slot spacing; zero is an explicit no-op.
 * @param plan Unmodified matching checked formula query result.
 * @param workspace Disjoint caller integer/packing storage; see file contract.
 * @param report Mandatory reset-before-preflight report; INFO stays absent.
 * @return OK or structural error; no numerical singularity test is performed.
 */
ASC_DENSE_LAPACK_EXPORT Status Laswp(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    index_t first_row, extent_t row_count, RawLapackPivotView pivots,
    index_t pivot_increment, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex LASWP conversion and packing capacities.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param first_row Zero-based first selected matrix row, at most m.
 * @param row_count Number of selected consecutive rows, at most m-first_row.
 * @param pivots Unchanged raw one-based sequential swap payload; see file.
 * @param pivot_increment Signed raw-slot spacing and application direction.
 * @return Metadata-bound caller plan or structural error without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix, index_t first_row,
    extent_t row_count, RawLapackPivotView pivots, index_t pivot_increment);
/** @brief Applies single complex sequential row swaps without allocation.
 * @param provider Explicit reference selection with no fallback.
 * @param matrix Mutable m-by-n matrix; padding is preserved.
 * @param first_row Zero-based first selected row.
 * @param row_count Number of selected consecutive rows.
 * @param pivots Immutable one-based raw selected row destinations.
 * @param pivot_increment Signed slot spacing; zero is an explicit no-op.
 * @param plan Unmodified matching checked formula query result.
 * @param workspace Disjoint caller integer/packing storage; see file contract.
 * @param report Mandatory reset-before-preflight report; INFO stays absent.
 * @return OK or structural error; no numerical singularity test is performed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Laswp(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix, index_t first_row,
      extent_t row_count, RawLapackPivotView pivots, index_t pivot_increment,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex LASWP conversion and packing capacities.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param first_row Zero-based first selected matrix row, at most m.
 * @param row_count Number of selected consecutive rows, at most m-first_row.
 * @param pivots Unchanged raw one-based sequential swap payload; see file.
 * @param pivot_increment Signed raw-slot spacing and application direction.
 * @return Metadata-bound caller plan or structural error without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaswpWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix, index_t first_row,
    extent_t row_count, RawLapackPivotView pivots, index_t pivot_increment);
/** @brief Applies double complex sequential row swaps without allocation.
 * @param provider Explicit reference selection with no fallback.
 * @param matrix Mutable m-by-n matrix; padding is preserved.
 * @param first_row Zero-based first selected row.
 * @param row_count Number of selected consecutive rows.
 * @param pivots Immutable one-based raw selected row destinations.
 * @param pivot_increment Signed slot spacing; zero is an explicit no-op.
 * @param plan Unmodified matching checked formula query result.
 * @param workspace Disjoint caller integer/packing storage; see file contract.
 * @param report Mandatory reset-before-preflight report; INFO stays absent.
 * @return OK or structural error; no numerical singularity test is performed.
 */
ASC_DENSE_LAPACK_EXPORT Status
Laswp(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix, index_t first_row,
      extent_t row_count, RawLapackPivotView pivots, index_t pivot_increment,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single real LAQGE caller packing without applying scales.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param row_scales Immutable contiguous real R, exact m entries when used.
 * @param column_scales Immutable contiguous real C, exact n entries when used.
 * @param statistics Immutable finite source decision data; see file contract.
 * @param applied Live disjoint host output object, unchanged by this query.
 * @return Checked metadata/statistic-bound plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics,
    const LapackEquilibration& applied);
/** @brief Applies single real source-selected row/column equilibration.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Mutable A; mathematical entries only, preserving padding.
 * @param row_scales Immutable R; unused values are not read.
 * @param column_scales Immutable C; unused values are not read.
 * @param statistics Immutable ROWCND/COLCND/AMAX selecting the source branch.
 * @param applied Actual N/R/C/B result; empty matrices produce kNone.
 * @param plan Unmodified matching query, including exact statistic bits.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory reset-before-preflight report; INFO always absent.
 * @return OK, structural error, kProvider for invalid EQUED, or kNumerical
 * with an accuracy warning for nonfinite scaled values; see file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Laqge(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics,
    LapackEquilibration& applied, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real LAQGE caller packing without applying scales.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param row_scales Immutable contiguous real R, exact m entries when used.
 * @param column_scales Immutable contiguous real C, exact n entries when used.
 * @param statistics Immutable finite source decision data; see file contract.
 * @param applied Live disjoint host output object, unchanged by this query.
 * @return Checked metadata/statistic-bound plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics,
    const LapackEquilibration& applied);
/** @brief Applies double real source-selected row/column equilibration.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Mutable A; mathematical entries only, preserving padding.
 * @param row_scales Immutable R; unused values are not read.
 * @param column_scales Immutable C; unused values are not read.
 * @param statistics Immutable ROWCND/COLCND/AMAX selecting the source branch.
 * @param applied Actual N/R/C/B result; empty matrices produce kNone.
 * @param plan Unmodified matching query, including exact statistic bits.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory reset-before-preflight report; INFO always absent.
 * @return OK, structural error, kProvider for invalid EQUED, or kNumerical
 * with an accuracy warning for nonfinite scaled values; see file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status Laqge(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics,
    LapackEquilibration& applied, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex LAQGE caller packing without applying scales.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param row_scales Immutable contiguous real R, exact m entries when used.
 * @param column_scales Immutable contiguous real C, exact n entries when used.
 * @param statistics Immutable finite source decision data; see file contract.
 * @param applied Live disjoint host output object, unchanged by this query.
 * @return Checked metadata/statistic-bound plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<const float> row_scales,
    DenseBlasVectorView<const float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics,
    const LapackEquilibration& applied);
/** @brief Applies single complex source-selected real equilibration scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Mutable A; mathematical entries only, preserving padding.
 * @param row_scales Immutable R; unused values are not read.
 * @param column_scales Immutable C; unused values are not read.
 * @param statistics Immutable ROWCND/COLCND/AMAX selecting the source branch.
 * @param applied Actual N/R/C/B result; empty matrices produce kNone.
 * @param plan Unmodified matching query, including exact statistic bits.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory reset-before-preflight report; INFO always absent.
 * @return OK, structural error, kProvider for invalid EQUED, or kNumerical
 * with an accuracy warning for nonfinite scaled values; see file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Laqge(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<const float> row_scales,
      DenseBlasVectorView<const float> column_scales,
      const LapackEquilibrationStatistics<float>& statistics,
      LapackEquilibration& applied, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex LAQGE caller packing without applying scales.
 * @param provider Explicit checked provider; no foreign query occurs.
 * @param matrix Unchanged m-by-n host matrix in either layout.
 * @param row_scales Immutable contiguous real R, exact m entries when used.
 * @param column_scales Immutable contiguous real C, exact n entries when used.
 * @param statistics Immutable finite source decision data; see file contract.
 * @param applied Live disjoint host output object, unchanged by this query.
 * @return Checked metadata/statistic-bound plan or structural error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryLaqgeWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<const double> row_scales,
    DenseBlasVectorView<const double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics,
    const LapackEquilibration& applied);
/** @brief Applies double complex source-selected real equilibration scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Mutable A; mathematical entries only, preserving padding.
 * @param row_scales Immutable R; unused values are not read.
 * @param column_scales Immutable C; unused values are not read.
 * @param statistics Immutable ROWCND/COLCND/AMAX selecting the source branch.
 * @param applied Actual N/R/C/B result; empty matrices produce kNone.
 * @param plan Unmodified matching query, including exact statistic bits.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory reset-before-preflight report; INFO always absent.
 * @return OK, structural error, kProvider for invalid EQUED, or kNumerical
 * with an accuracy warning for nonfinite scaled values; see file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Laqge(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<const double> row_scales,
      DenseBlasVectorView<const double> column_scales,
      const LapackEquilibrationStatistics<double>& statistics,
      LapackEquilibration& applied, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_HELPERS_H_
