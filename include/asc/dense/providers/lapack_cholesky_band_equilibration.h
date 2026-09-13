#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_EQUILIBRATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_EQUILIBRATION_H_

/** @file
 * @brief Explicit Reference-LAPACK positive-definite band diagonal scaling.
 *
 * PBEQU computes S[i]=1/sqrt(real(A[i,i])) without applying the scales or
 * modifying A. It reads only real diagonal components, never off-diagonals,
 * padding, or the imaginary diagonal. This is not an SPD validation routine.
 * For SPD/Hermitian positive-definite A, the maximum diagonal equals the
 * largest matrix-element modulus. For arbitrary input, AMAX is only the raw
 * maximum real diagonal as computed by the pinned source, not a magnitude scan.
 *
 * Formula queries make no foreign call or numerical reads. Column-major
 * requires zero workspace; row-major requires n*(kd+1) live T entries in
 * kLayoutConversion, but only their diagonal slots are written/read. All input,
 * S, statistics, metadata and scratch objects are disjoint and caller-owned.
 * Output S has exactly n contiguous real entries, including for complex A.
 * Provider/context and every nonempty workspace role are checked before use.
 * No allocation, transfer, densification, synchronization or fallback occurs.
 *
 * Structural failure leaves all numerical outputs unchanged and INFO absent;
 * metadata aliasing prevents report reset, otherwise report is reset first.
 * On INFO=0, S/SCOND/AMAX are raw successful outputs without a finiteness
 * guarantee. At n=0 the actual foreign call sets SCOND=1 and AMAX=0 and leaves
 * S untouched. Positive INFO<=n identifies a nonpositive real diagonal: raw S
 * then contains all original real diagonals, AMAX is computed, and SCOND
 * remains unchanged and invalid. The report is documented partial, not
 * reusable-factor provenance. Negative/impossible INFO is an unusable provider
 * failure. Raw NaN arithmetic follows the pinned source; INFO=0 is not proof of
 * finite scales.
 */

#include <complex>
#include <concepts>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Caller-owned real PBEQU diagnostics, not a full matrix norm scan.
 * @tparam Real float or double matching the matrix's real component type.
 */
template <typename Real>
  requires(std::same_as<Real, float> || std::same_as<Real, double>)
struct LapackBandEquilibrationStatistics {
  Real scale_condition = 0;   ///< SCOND; min(S)/max(S), valid on INFO=0 only.
  Real absolute_maximum = 0;  ///< AMAX; actual raw maximum real diagonal.
};

/** @brief Computes the checked fixed PBEQU diagonal-packing plan.
 * @param provider Explicit provider; the query makes no foreign call.
 * @param matrix Immutable band descriptor in either layout, unchanged.
 * @param scales Exact n-entry contiguous real output descriptor, unchanged.
 * @param statistics Live disjoint host diagnostics object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const float> matrix,
    DenseBlasVectorView<float> scales,
    const LapackBandEquilibrationStatistics<float>& statistics);
/** @brief Computes real diagonal scaling factors without applying them.
 * @param provider Explicit checked reference provider and context.
 * @param matrix Immutable band A; only real diagonal components are read.
 * @param scales Contiguous real S output; see positive-INFO partial semantics.
 * @param statistics Caller SCOND/AMAX outputs with routine-specific validity.
 * @param plan Unmodified matching fixed formula plan.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory raw INFO, partial-validity and failure diagnostics.
 * @return OK, structural error, kNumerical for a nonpositive real diagonal,
 * or kProvider for invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbequ(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const float> matrix,
      DenseBlasVectorView<float> scales,
      LapackBandEquilibrationStatistics<float>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the checked fixed PBEQU diagonal-packing plan.
 * @param provider Explicit provider; the query makes no foreign call.
 * @param matrix Immutable band descriptor in either layout, unchanged.
 * @param scales Exact n-entry contiguous real output descriptor, unchanged.
 * @param statistics Live disjoint host diagnostics object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const double> matrix,
    DenseBlasVectorView<double> scales,
    const LapackBandEquilibrationStatistics<double>& statistics);
/** @brief Computes real diagonal scaling factors without applying them.
 * @param provider Explicit checked reference provider and context.
 * @param matrix Immutable band A; only real diagonal components are read.
 * @param scales Contiguous real S output; see positive-INFO partial semantics.
 * @param statistics Caller SCOND/AMAX outputs with routine-specific validity.
 * @param plan Unmodified matching fixed formula plan.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory raw INFO, partial-validity and failure diagnostics.
 * @return OK, structural error, kNumerical for a nonpositive real diagonal,
 * or kProvider for invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbequ(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const double> matrix,
      DenseBlasVectorView<double> scales,
      LapackBandEquilibrationStatistics<double>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the checked fixed PBEQU diagonal-packing plan.
 * @param provider Explicit provider; the query makes no foreign call.
 * @param matrix Immutable band descriptor in either layout, unchanged.
 * @param scales Exact n-entry contiguous real output descriptor, unchanged.
 * @param statistics Live disjoint host diagnostics object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> scales,
    const LapackBandEquilibrationStatistics<float>& statistics);
/** @brief Computes real diagonal scaling factors without applying them.
 * @param provider Explicit checked reference provider and context.
 * @param matrix Immutable band A; only real diagonal components are read.
 * @param scales Contiguous real S output; see positive-INFO partial semantics.
 * @param statistics Caller SCOND/AMAX outputs with routine-specific validity.
 * @param plan Unmodified matching fixed formula plan.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory raw INFO, partial-validity and failure diagnostics.
 * @return OK, structural error, kNumerical for a nonpositive real diagonal,
 * or kProvider for invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbequ(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<float>> matrix,
      DenseBlasVectorView<float> scales,
      LapackBandEquilibrationStatistics<float>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the checked fixed PBEQU diagonal-packing plan.
 * @param provider Explicit provider; the query makes no foreign call.
 * @param matrix Immutable band descriptor in either layout, unchanged.
 * @param scales Exact n-entry contiguous real output descriptor, unchanged.
 * @param statistics Live disjoint host diagnostics object, unchanged.
 * @return Identity-bound fixed plan, or structural/ABI/placement failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPbequWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteBandView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> scales,
    const LapackBandEquilibrationStatistics<double>& statistics);
/** @brief Computes real diagonal scaling factors without applying them.
 * @param provider Explicit checked reference provider and context.
 * @param matrix Immutable band A; only real diagonal components are read.
 * @param scales Contiguous real S output; see positive-INFO partial semantics.
 * @param statistics Caller SCOND/AMAX outputs with routine-specific validity.
 * @param plan Unmodified matching fixed formula plan.
 * @param workspace Disjoint caller-owned live scalar packing objects.
 * @param report Mandatory raw INFO, partial-validity and failure diagnostics.
 * @return OK, structural error, kNumerical for a nonpositive real diagonal,
 * or kProvider for invalid native INFO; see the file contract.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pbequ(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteBandView<const std::complex<double>> matrix,
      DenseBlasVectorView<double> scales,
      LapackBandEquilibrationStatistics<double>& statistics,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_BAND_EQUILIBRATION_H_
