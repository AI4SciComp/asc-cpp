#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EQUILIBRATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EQUILIBRATION_H_

/** @file
 * @brief Exact Reference-LAPACK compact general-band row/column equilibration.
 *
 * GBEQU computes scale factors without changing or applying scaling to AB.
 * Compact AB uses ReferenceGeneralBandView; expanded LU factor storage is a
 * different convention. All nonnegative bandwidths and rectangular/empty
 * shapes are supported within the checked source-integer arithmetic domain.
 * Scale vectors contain exactly m/n contiguous live underlying-real objects.
 * Complex magnitude is abs(real)+abs(imag), following the pinned routine.
 * Source clamping and raw ROWCND/COLCND/AMAX values are preserved. No decrease
 * in condition number or blanket finite-output guarantee is
 * added to the source behavior.
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
 * INFO=i<=m identifies an exactly zero row: AMAX is available, while neither
 * vector nor ratio is a usable scale result. INFO=m+j identifies a zero column:
 * R/ROWCND/AMAX are usable, while C/COLCND are not. These returns are
 * kNumerical/kSingular with documented partial output and the zero-based row
 * or column index; raw INFO distinguishes the two cases. No factor is
 * certified. Negative, impossible or unwritten INFO is kProvider/kUnusable.
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

/** @brief Queries zero-workspace capacities for single real GBEQU.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Executes actual single real GBEQU without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, exact-zero-row/column numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequ(const ReferenceLapackProvider& provider,
      ReferenceGeneralBandView<const float> matrix,
      DenseBlasVectorView<float> row_scales,
      DenseBlasVectorView<float> column_scales,
      LapackEquilibrationStatistics<float>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries zero-workspace capacities for double real GBEQU.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Executes actual double real GBEQU without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, exact-zero-row/column numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequ(const ReferenceLapackProvider& provider,
      ReferenceGeneralBandView<const double> matrix,
      DenseBlasVectorView<double> row_scales,
      DenseBlasVectorView<double> column_scales,
      LapackEquilibrationStatistics<double>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries zero-workspace capacities for single complex GBEQU.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Executes actual single complex GBEQU without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, exact-zero-row/column numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequ(const ReferenceLapackProvider& provider,
      ReferenceGeneralBandView<const std::complex<float>> matrix,
      DenseBlasVectorView<float> row_scales,
      DenseBlasVectorView<float> column_scales,
      LapackEquilibrationStatistics<float>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries zero-workspace capacities for double complex GBEQU.
 * @param provider Explicit serial reference provider, with no fallback.
 * @param matrix Unchanged compact m-by-n AB, preserving ignored corners.
 * @param row_scales Unchanged exact m-entry real output descriptor.
 * @param column_scales Unchanged exact n-entry real output descriptor.
 * @param statistics Unchanged live disjoint ROWCND/COLCND/AMAX object.
 * @return Checked formula plan or structural failure; no values are read.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbequWorkspace(
    const ReferenceLapackProvider& provider,
    ReferenceGeneralBandView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Executes actual double complex GBEQU without applying its scaling.
 * @param provider Explicit serial reference provider matching the plan.
 * @param matrix Immutable compact band coefficients; no densification.
 * @param row_scales Contiguous m-entry R output, partial as described above.
 * @param column_scales Contiguous n-entry C output, partial as described above.
 * @param statistics Live disjoint raw source diagnostics, with partial
 * validity.
 * @param plan Unmodified matching source/scalar/shape/provider formula plan.
 * @param workspace Caller workspace; this route needs no numerical entries.
 * @param report Mandatory disjoint outcome retaining the exact signed INFO.
 * @return OK, structural error, exact-zero-row/column numerical failure, or
 * provider defect. Source nonfinite arithmetic retains its documented limits.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbequ(const ReferenceLapackProvider& provider,
      ReferenceGeneralBandView<const std::complex<double>> matrix,
      DenseBlasVectorView<double> row_scales,
      DenseBlasVectorView<double> column_scales,
      LapackEquilibrationStatistics<double>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_EQUILIBRATION_H_
