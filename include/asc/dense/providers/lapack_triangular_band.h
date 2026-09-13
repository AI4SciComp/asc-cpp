#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_BAND_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_BAND_H_

/** @file
 * @brief Explicit pinned CPU solves with triangular band coefficients.
 *
 * TBTRS solves op(A)*X=B, preserving A and overwriting B. The checked
 * LapackTriangularBandView carries order, bandwidth, triangle and physical
 * encoding. Bandwidth may equal or exceed order. Unit diagonal slots and
 * unused band corners/padding are never read. No Hermitian completion,
 * full-matrix validation or implicit densification occurs. N/T/C retain their
 * source meanings; complex transpose and conjugate transpose are distinct.
 *
 * A and B layouts are independent. Row-major A needs n*(kd+1) live T objects
 * in caller kLayoutConversion workspace; column-major A is direct. Row-major
 * B adds n*nrhs live T objects; column-major B is direct. Other workspace roles
 * need no capacity. Formula queries read metadata only and call no provider.
 * Zero-order calls need no scratch and complete locally with absent INFO.
 * For n>0 and nrhs=0 the provider still scans a nonunit diagonal. Row-major A
 * then needs n*(kd+1) slots, but only diagonal coefficients are copied.
 * Unit zero-RHS calls read neither operand and need no layout workspace.
 *
 * Plans bind every scalar, option, dimension, original layout/stride, actual
 * foreign leading dimension and provider/build/integer ABI. Source-derived
 * bounds include KD+1, diagonal/RHS loop terminals and TBSV's selected J+KD
 * intermediates. Unused foreign arguments on local zero-order completion
 * are not narrowed. All preflight precedes numeric mutation/provider entry.
 * Numeric ranges, workspace regions and provider/plan/workspace/report
 * metadata must be disjoint. Conservative ranges include full band-table
 * and RHS padding. Detected metadata alias preserves the report; otherwise
 * the report resets before remaining preflight. Structural rejection leaves
 * numeric buffers and workspace unchanged.
 *
 * Positive INFO identifies an exactly zero nonunit diagonal and returns
 * kNumerical/kSingular/kUnchanged, retaining raw INFO and a zero-based
 * diagnostic index. Original A and B are unchanged on that source-defined
 * failure, while workspace can hold packing values. Negative, unwritten or
 * impossible INFO is a provider defect: preserve full-width raw INFO, mark
 * outputs unusable and withhold row-major B publication. Direct native
 * writes cannot be rolled back. Near singularity and nonfinite arithmetic
 * follow the pinned source; INFO=0 is not a finiteness or accuracy certificate.
 * Preserve original inputs for independent residual checks.
 *
 * Live storage must survive the synchronous call. Only accessible host or
 * pinned-host storage and the explicitly selected optional CPU provider are
 * supported. No allocation, precision conversion, transfer, fallback,
 * synchronization or global error-handler mutation occurs. Disjoint calls
 * are reentrant. Link ASC::dense_lapack; native capability remains separate.
 */

#include <complex>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Computes the metadata-only workspace formula for Tbtrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure, with no
 * coefficient reads, provider call or allocation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation, LapackTriangularBandView<const float> a,
    DenseBlasMatrixView<float> b);

/** @brief Solves op(A)*X=B through exact reference TBTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tbtrs(const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
      DenseBlasTranspose operation, LapackTriangularBandView<const float> a,
      DenseBlasMatrixView<float> b, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tbtrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure, with no
 * coefficient reads, provider call or allocation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation, LapackTriangularBandView<const double> a,
    DenseBlasMatrixView<double> b);

/** @brief Solves op(A)*X=B through exact reference TBTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tbtrs(const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
      DenseBlasTranspose operation, LapackTriangularBandView<const double> a,
      DenseBlasMatrixView<double> b, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tbtrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure, with no
 * coefficient reads, provider call or allocation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation,
    LapackTriangularBandView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b);

/** @brief Solves op(A)*X=B through exact reference TBTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status Tbtrs(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation,
    LapackTriangularBandView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tbtrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure, with no
 * coefficient reads, provider call or allocation.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
    DenseBlasTranspose operation,
    LapackTriangularBandView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b);

/** @brief Solves op(A)*X=B through exact reference TBTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed triangular band coefficients; stored unit diagonal is
 * unused.
 * @param b Borrowed n-by-nrhs RHS with independent layout and unchanged
 * padding.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tbtrs(const ReferenceLapackProvider& provider, DenseBlasDiagonal diagonal,
      DenseBlasTranspose operation,
      LapackTriangularBandView<const std::complex<double>> a,
      DenseBlasMatrixView<std::complex<double>> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_BAND_H_
