#ifndef ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_EQUILIBRATION_H_
#define ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_EQUILIBRATION_H_

/** @file
 * @brief Explicit diagonal-only Reference-LAPACK positive-definite scaling.
 *
 * POEQU computes S(i)=1/sqrt(real(A(i,i))). POEQUB instead uses the pinned
 * radix-power formula with truncation of its logarithmic exponent. These
 * routines only compute scales; they neither change A nor apply scaling.
 * Only real diagonal components are mathematical input. Off-diagonal entries,
 * padding and complex imaginary diagonals are never inspected.
 * On success AMAX is the maximum real diagonal and SCOND is
 * sqrt(min(real(diagonal)))/sqrt(max(real(diagonal))). The pinned POEQUB
 * retains this unquantized SCOND formula: it need not equal the ratio of
 * its returned radix-power scale extrema.
 *
 * Formula-only queries do not read numerical values or call the provider.
 * Column-major requires no scratch. Row-major uses n*n caller-owned live
 * scalar objects in kLayoutConversion; only diagonal objects are written/read
 * there. No implicit allocation, transfer, densification, synchronization or
 * fallback occurs. Plans bind scalar/routine, shape, both actual foreign and
 * original ASC strides, layout, vector length/increment and provider identity.
 * Host storage must be accessible to the explicit serial provider.
 *
 * S, SCOND and AMAX are disjoint from A, each other, all workspace and live
 * provider/plan/report metadata. Lifetimes cover the call; concurrent calls
 * require disjoint mutable storage and reports. Structural failure changes no
 * numerical output and does not call the provider. Reports reset before
 * preflight, except a metadata alias error leaves an aliased report untouched.
 *
 * POEQUB rejects nonfinite real diagonal input before its LOG-to-INTEGER
 * conversion. Finite nonpositive diagonals are passed through to the provider.
 * Positive INFO identifies the exact first nonpositive diagonal: AMAX and raw
 * copied diagonals in S are retained, but S is not a valid scale vector and
 * SCOND remains unchanged. No successful factor is certified. Negative or
 * impossible INFO is a provider defect with unusable output. Exact INFO is
 * preserved; the diagnostic index is zero-based only for positive INFO.
 *
 * After INFO=0, nonfinite/zero scale results or nonfinite statistics produce
 * kNumerical/kAccuracyWarning with raw outputs retained as documented partial,
 * not a finiteness guarantee inferred from INFO. For n=0, SCOND=1, AMAX=0 and
 * S is untouched, without a provider call or fabricated INFO.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Queries single real POEQU diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix, DenseBlasVectorView<float> scales,
    const float& scale_condition, const float& absolute_maximum);

/** @brief Executes single real POEQU without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Poequ(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<const float> matrix,
                                     DenseBlasVectorView<float> scales,
                                     float& scale_condition,
                                     float& absolute_maximum,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real POEQUB diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const float> matrix, DenseBlasVectorView<float> scales,
    const float& scale_condition, const float& absolute_maximum);

/** @brief Executes single real POEQUB without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Poequb(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<const float> matrix,
                                      DenseBlasVectorView<float> scales,
                                      float& scale_condition,
                                      float& absolute_maximum,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries double real POEQU diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum);

/** @brief Executes double real POEQU without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Poequ(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<const double> matrix,
                                     DenseBlasVectorView<double> scales,
                                     double& scale_condition,
                                     double& absolute_maximum,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real POEQUB diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const double> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum);

/** @brief Executes double real POEQUB without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status Poequb(const ReferenceLapackProvider& provider,
                                      DenseBlasMatrixView<const double> matrix,
                                      DenseBlasVectorView<double> scales,
                                      double& scale_condition,
                                      double& absolute_maximum,
                                      const LapackWorkspacePlan& plan,
                                      const LapackWorkspace& workspace,
                                      LapackReport& report);

/** @brief Queries single complex POEQU diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> scales, const float& scale_condition,
    const float& absolute_maximum);

/** @brief Executes single complex POEQU without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Poequ(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<float>> matrix,
      DenseBlasVectorView<float> scales, float& scale_condition,
      float& absolute_maximum, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex POEQUB diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<float>> matrix,
    DenseBlasVectorView<float> scales, const float& scale_condition,
    const float& absolute_maximum);

/** @brief Executes single complex POEQUB without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Poequb(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const std::complex<float>> matrix,
       DenseBlasVectorView<float> scales, float& scale_condition,
       float& absolute_maximum, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex POEQU diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum);

/** @brief Executes double complex POEQU without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Poequ(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const std::complex<double>> matrix,
      DenseBlasVectorView<double> scales, double& scale_condition,
      double& absolute_maximum, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex POEQUB diagonal packing.
 * @param provider Explicit checked reference provider, unchanged.
 * @param matrix Immutable square matrix in either full layout.
 * @param scales Disjoint contiguous n-entry underlying-real output, unread.
 * @param scale_condition Disjoint live host SCOND output object, unread.
 * @param absolute_maximum Disjoint live host AMAX output object, unread.
 * @return Metadata-bound formula plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPoequbWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<const std::complex<double>> matrix,
    DenseBlasVectorView<double> scales, const double& scale_condition,
    const double& absolute_maximum);

/** @brief Executes double complex POEQUB without applying scales.
 * @param provider Explicit checked reference provider; no fallback.
 * @param matrix Immutable square matrix; only real diagonal input is used.
 * @param scales Disjoint contiguous n-entry underlying-real S output.
 * @param scale_condition Disjoint live host SCOND; unchanged on positive INFO.
 * @param absolute_maximum Disjoint live host AMAX diagonal maximum.
 * @param plan Unmodified matching formula query result.
 * @param workspace Explicit disjoint live scalar packing storage.
 * @param report Mandatory failure-surviving diagnostics; see alias exception.
 * @return OK, structural error, exact nonpositive diagonal failure, numerical
 * warning, or provider defect with the file-level partial-output semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Poequb(const ReferenceLapackProvider& provider,
       DenseBlasMatrixView<const std::complex<double>> matrix,
       DenseBlasVectorView<double> scales, double& scale_condition,
       double& absolute_maximum, const LapackWorkspacePlan& plan,
       const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_CHOLESKY_EQUILIBRATION_H_
