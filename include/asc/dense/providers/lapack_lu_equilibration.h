#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_EQUILIBRATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_EQUILIBRATION_H_

/** @file
 * @brief Explicit allocation-free reference general-matrix equilibration.
 *
 * Upstream GEEQU/GEEQUB have no numerical WORK or LWORK query. ASC's queries
 * compute exact checked caller packing capacities, without a foreign call or
 * numerical mutation. Column-major needs zero entries; row-major requires
 * m*n live scalar entries in kLayoutConversion and packs the original A into
 * column-major order before the same row-then-column equilibration. This is not
 * an operation on transpose(A) with swapped scale vectors. Plans bind routine,
 * scalar, shape, leading dimension, layout, output increments/counts and exact
 * provider build/ABI. Execution revalidates unmodified plans before packing.
 *
 * These calls compute scale factors without applying them or changing A.
 * There is no allocation, transfer, hidden packing, synchronization or
 * fallback. Matrices may be column-major or row-major host/pinned-host. Output
 * real vectors are contiguous exact m/n entries and disjoint from each other,
 * A, and the live host statistics object. All lifetimes cover the call;
 * independent calls with disjoint outputs/reports are reentrant.
 *
 * Complex magnitude means abs(real)+abs(imag), not Euclidean modulus. GEEQU
 * computes reciprocal clamped row/column maxima. GEEQUB restricts factors to
 * powers of the floating radix. In the pinned 3.12.1 implementation GEEQUB's
 * AMAX is the maximum radix-quantized row maximum before clamping/inversion,
 * not the exact largest original magnitude; GEEQU's AMAX is that exact largest
 * magnitude. No reduction in condition number is promised. Nonfinite arithmetic
 * follows the pinned provider and does not acquire a finiteness guarantee.
 *
 * Structural failure leaves every numerical output unchanged, called_provider
 * false and raw INFO absent. Negative INFO is a provider-contract defect, with
 * unusable output. Positive INFO<=m identifies a zero computed row scale; AMAX
 * is available, but R/C and both condition ratios are not valid scale results.
 * Positive INFO>m identifies a zero computed column scale; R, ROWCND and AMAX
 * are valid, while C/COLCND are not. The report retains exact raw INFO and a
 * zero-based index within that row or column; output validity is documented
 * partial. No singular result is tagged as a reusable LU factor. On success
 * all nonempty scale outputs/statistics are complete. With m=0 or n=0 no
 * foreign call occurs, ratios become one, AMAX zero, and R/C remain untouched.
 * GEEQU's positive INFO identifies an exactly zero input row/column. GEEQUB
 * instead reports LapackOutcome::kPartialResult: the pinned LP64 GNU power
 * helper can produce
 * zero radix scales from nonzero subnormal input, which is a known provider
 * numerical limitation, not proof of singular A or successful equilibration.
 */

#include <complex>
#include <concepts>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Caller-owned real equilibration diagnostics with routine-specific
 * partial validity.
 * @tparam Real float or double, matching the matrix's underlying real scalar.
 */
template <typename Real>
  requires(std::same_as<Real, float> || std::same_as<Real, double>)
struct LapackEquilibrationStatistics {
  Real row_condition = 0;     ///< ROWCND; valid on INFO=0 or INFO>m.
  Real column_condition = 0;  ///< COLCND; valid only on INFO=0.
  Real absolute_maximum = 0;  ///< AMAX; exact for GEEQU, quantized for GEEQUB.
};

/** @brief Computes exact single real GEEQU packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Computes single real GEEQU scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical for a zero row/column.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequ(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const float> matrix,
      DenseBlasVectorView<float> row_scales,
      DenseBlasVectorView<float> column_scales,
      LapackEquilibrationStatistics<float>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact single real GEEQUB packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Computes single real GEEQUB scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical with a partial-result report
 * for a zero computed radix scale, including the nonzero-input limitation.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequb(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const float> matrix,
       DenseBlasVectorView<float> row_scales,
       DenseBlasVectorView<float> column_scales,
       LapackEquilibrationStatistics<float>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Computes exact double real GEEQU packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Computes double real GEEQU scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical for a zero row/column.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequ(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const double> matrix,
      DenseBlasVectorView<double> row_scales,
      DenseBlasVectorView<double> column_scales,
      LapackEquilibrationStatistics<double>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact double real GEEQUB packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Computes double real GEEQUB scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical with a partial-result report
 * for a zero computed radix scale, including the nonzero-input limitation.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequb(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const double> matrix,
       DenseBlasVectorView<double> row_scales,
       DenseBlasVectorView<double> column_scales,
       LapackEquilibrationStatistics<double>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Computes exact single complex GEEQU packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Computes single complex GEEQU scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical for a zero row/column.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequ(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<float>> matrix,
      DenseBlasVectorView<float> row_scales,
      DenseBlasVectorView<float> column_scales,
      LapackEquilibrationStatistics<float>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact single complex GEEQUB packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> row_scales,
    DenseBlasVectorView<float> column_scales,
    const LapackEquilibrationStatistics<float>& statistics);
/** @brief Computes single complex GEEQUB scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical with a partial-result report
 * for a zero computed radix scale, including the nonzero-input limitation.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequb(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const std::complex<float>> matrix,
       DenseBlasVectorView<float> row_scales,
       DenseBlasVectorView<float> column_scales,
       LapackEquilibrationStatistics<float>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

/** @brief Computes exact double complex GEEQU packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Computes double complex GEEQU scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical for a zero row/column.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequ(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<double>> matrix,
      DenseBlasVectorView<double> row_scales,
      DenseBlasVectorView<double> column_scales,
      LapackEquilibrationStatistics<double>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact double complex GEEQUB packing capacities.
 * @param provider Explicit checked provider; this query makes no foreign call.
 * @param matrix Immutable m-by-n host/pinned-host matrix in either layout.
 * @param row_scales Contiguous m-entry real output descriptor, unchanged.
 * @param column_scales Contiguous n-entry real output descriptor, unchanged.
 * @param statistics Live disjoint host statistics object, unchanged.
 * @return Matching checked zero/packing plan or structural validation failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGeequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> row_scales,
    DenseBlasVectorView<double> column_scales,
    const LapackEquilibrationStatistics<double>& statistics);
/** @brief Computes double complex GEEQUB scale factors without applying them.
 * @param provider Explicit checked reference selection; no implicit discovery.
 * @param matrix Immutable m-by-n host/pinned-host A, preserving layout/padding.
 * @param row_scales Contiguous m-entry real R output, with no implicit resize.
 * @param column_scales Contiguous n-entry real C output, disjoint from R/A.
 * @param statistics Live disjoint host ROWCND/COLCND/AMAX output; see file
 * contract.
 * @param plan Unmodified metadata-matching checked formula query result.
 * @param workspace Disjoint caller-owned packing objects; zero for
 * column-major.
 * @param report Mandatory reset-before-preflight, failure-surviving
 * diagnostics.
 * @return OK, structural error, or kNumerical with a partial-result report
 * for a zero computed radix scale, including the nonzero-input limitation.
 */
ASC_DENSE_LAPACK_EXPORT Status
Geequb(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const std::complex<double>> matrix,
       DenseBlasVectorView<double> row_scales,
       DenseBlasVectorView<double> column_scales,
       LapackEquilibrationStatistics<double>& statistics,
       const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
       LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_EQUILIBRATION_H_
