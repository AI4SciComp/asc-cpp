#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EQUILIBRATION_RADIX_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EQUILIBRATION_RADIX_H_

/** @file
 * @brief Exact Reference-LAPACK compact general-band radix-power row/column
 * equilibration.
 *
 * GBEQUB computes scale factors without changing or applying scaling to AB.
 * Compact AB uses ReferenceGeneralBandView; expanded LU factor storage is a
 * different convention. All nonnegative bandwidths and rectangular/empty
 * shapes are supported within the checked source-integer arithmetic domain.
 * Scale vectors contain exactly m/n contiguous live underlying-real objects.
 * Complex magnitude is abs(real)+abs(imag), following the pinned routine.
 * Row/column maxima are quantized by the exact native RADIX**INT(LOG(value)/
 * LOG(RADIX)) arithmetic, then clamped and inverted. This is the actual GBEQUB
 * routine, not GBEQU followed by local rounding. Raw ROWCND/COLCND/AMAX values
 * are retained: the pinned source assigns AMAX after row quantization, which
 * can differ from the documented original maximum, and clamped ratios can
 * violate their mathematical ratio property. No reduction in condition number
 * or blanket finite-output guarantee is added. Nonfinite arithmetic, including
 * LOG-to-integer conversion, follows the pinned provider's platform behavior.
 * Ignored corner/padding values are not scanned or read as coefficients.
 *
 * Queries are metadata-only checked formulas; the source has no WORK query
 * and these plans need zero numerical workspace. Plans bind source routine,
 * scalar, dimensions, bandwidths, physical stride, vector counts/increments
 * and explicit provider identity. All operands, statistics, metadata and
 * nonempty scratch are live and disjoint; host/pinned-host access is checked.
 * No allocation, packing, transfer, synchronization, fallback or global
 * handler change occurs. Independent operations need disjoint writable data.
 *
 * Metadata aliases preserve the caller report. After metadata alias checks,
 * the report resets before remaining preflight. Structural failure changes
 * no numerical or workspace byte and makes no native call. Every admitted
 * call, including empty shapes, executes the actual source and retains INFO.
 * Empty source returns ratios one and AMAX zero, leaving scale vectors alone.
 * INFO=i<=m identifies a zero source-computed row scale: AMAX is available,
 * while neither vector nor ratio is a usable scale result. INFO=m+j identifies
 * a zero source-computed column scale: R/ROWCND/AMAX are usable, while C/COLCND
 * are not. These returns are kNumerical/kSingular with documented partial
 * output and the zero-based row or column index; raw INFO distinguishes the two
 * cases. No factor is certified. Negative, impossible or unwritten INFO is
 * kProvider/kUnusable.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_general_band.h"
#include "asc/dense/providers/lapack_lu_equilibration.h"

namespace asc {

/** @brief Queries zero-workspace capacities for single real GBEQUB.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Executes actual single real GBEQUB without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, zero-source-scale numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequb(const ReferenceLapackProvider& provider,
       ReferenceGeneralBandView<const float> matrix,
       DenseBlasVectorView<float> row_scales,
       DenseBlasVectorView<float> column_scales,
       LapackEquilibrationStatistics<float>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries zero-workspace capacities for double real GBEQUB.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Executes actual double real GBEQUB without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, zero-source-scale numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequb(const ReferenceLapackProvider& provider,
       ReferenceGeneralBandView<const double> matrix,
       DenseBlasVectorView<double> row_scales,
       DenseBlasVectorView<double> column_scales,
       LapackEquilibrationStatistics<double>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries zero-workspace capacities for single complex GBEQUB.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Executes actual single complex GBEQUB without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, zero-source-scale numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequb(const ReferenceLapackProvider& provider,
       ReferenceGeneralBandView<const std::complex<float>> matrix,
       DenseBlasVectorView<float> row_scales,
       DenseBlasVectorView<float> column_scales,
       LapackEquilibrationStatistics<float>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Queries zero-workspace capacities for double complex GBEQUB.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequbWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Executes actual double complex GBEQUB without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, zero-source-scale numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequb(const ReferenceLapackProvider& provider,
       ReferenceGeneralBandView<const std::complex<double>> matrix,
       DenseBlasVectorView<double> row_scales,
       DenseBlasVectorView<double> column_scales,
       LapackEquilibrationStatistics<double>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EQUILIBRATION_RADIX_H_
