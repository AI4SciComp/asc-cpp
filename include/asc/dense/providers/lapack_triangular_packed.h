#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_PACKED_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_PACKED_H_

/** @file
 * @brief Explicit pinned CPU packed triangular inverse and reusable solves.
 *
 * TPTRI overwrites the stored triangular coefficients with their inverse.
 * TPTRS solves op(A)*X=B, preserving A and overwriting the full RHS matrix B.
 * A is an existing checked DenseBlasPackedMatrixView with n*(n+1)/2 slots:
 * upper/lower and row/column packed orders follow the Dense BLAS contract.
 * This is ordinary packed storage, not a full, banded or RFP encoding.
 * Unit diagonal slots are never read or changed; no full-matrix validation
 * or implicit densification occurs. Solves accept N/T/C, including actual
 * complex conjugation. Real T and C retain their equal mathematical meaning.
 *
 * A and B layouts are independent. Row-packed A uses p=n*(n+1)/2 live T
 * objects in explicit caller kLayoutConversion workspace; column-packed A is
 * direct. Row-major B adds n*nrhs live T objects; column-major B is direct.
 * No other workspace role requires capacity. Zero-order operations need no
 * scratch and complete locally with absent native INFO. With n>0 and nrhs=0,
 * TPTRS still enters the provider and scans a nonunit diagonal: row-packed A
 * then needs p slots but only its diagonal values are copied. Unit zero-RHS
 * calls read neither numeric operand and require no layout workspace.
 *
 * Formula queries inspect metadata only, call no provider, and bind every
 * scalar, option, shape, original layout/stride, actual foreign dimension and
 * exact provider/build/integer ABI. Native bounds cover actual packed cursor
 * intermediates and loop terminals. Stale plans fail. Numeric ranges, supplied
 * workspace regions and provider/plan/workspace/report metadata must be
 * disjoint; conservative ranges include RHS padding. Detected metadata alias
 * preserves the report. Otherwise it is reset before remaining preflight.
 * Structural rejection leaves numeric buffers and workspace unchanged.
 *
 * Both routines report positive INFO for an exactly zero nonunit diagonal:
 * kNumerical/kSingular/kUnchanged, with a zero-based diagnostic index. Original
 * A and B remain unchanged on that source-defined failure; workspace can hold
 * packing values. Negative, unwritten or impossible INFO is a provider defect:
 * preserve full-width raw INFO, mark outputs unusable and withhold row-packed
 * publication. Direct native writes cannot be rolled back. Nonfinite
 * arithmetic follows the pinned source; INFO=0 is not a finiteness,
 * invertibility or conditioning certificate. No factor-family provenance is
 * fabricated. Preserve original inputs for independent residual checks.
 *
 * Storage and live objects must survive the synchronous call. Only accessible
 * host/pinned-host storage and the explicitly selected optional CPU provider
 * are supported. No allocation, precision conversion, transfer, fallback,
 * synchronization or global error-handler mutation occurs. Disjoint calls are
 * reentrant. Link ASC::dense_lapack; native coverage is a separate contract.
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

/** @brief Computes the metadata-only workspace formula for Tptri.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasPackedMatrixView<float> a);

/** @brief Inverts packed coefficients through exact reference TPTRI.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status Tptri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasPackedMatrixView<float> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const float> a, DenseBlasMatrixView<float> b);

/** @brief Solves op(A)*X=B through exact reference TPTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
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
Tptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasPackedMatrixView<const float> a, DenseBlasMatrixView<float> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptri.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasPackedMatrixView<double> a);

/** @brief Inverts packed coefficients through exact reference TPTRI.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status Tptri(const ReferenceLapackProvider& provider,
                                     DenseBlasTriangle triangle,
                                     DenseBlasDiagonal diagonal,
                                     DenseBlasPackedMatrixView<double> a,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const double> a, DenseBlasMatrixView<double> b);

/** @brief Solves op(A)*X=B through exact reference TPTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
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
Tptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasPackedMatrixView<const double> a, DenseBlasMatrixView<double> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptri.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<std::complex<float>> a);

/** @brief Inverts packed coefficients through exact reference TPTRI.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal,
      DenseBlasPackedMatrixView<std::complex<float>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b);

/** @brief Solves op(A)*X=B through exact reference TPTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
 * padding.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status Tptrs(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const std::complex<float>> a,
    DenseBlasMatrixView<std::complex<float>> b, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptri.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptriWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal,
    DenseBlasPackedMatrixView<std::complex<double>> a);

/** @brief Inverts packed coefficients through exact reference TPTRI.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param plan Unmodified formula plan matching this provider and descriptors.
 * @param workspace Disjoint live scalar objects; exact roles follow the file
 * contract.
 * @param report Mandatory surviving raw INFO, outcome and output-validity
 * report.
 * @return OK or structural/numerical/provider failure with publication and
 * lifetime semantics defined above; success is not a numerical certificate.
 */
ASC_DENSE_LAPACK_EXPORT Status
Tptri(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal,
      DenseBlasPackedMatrixView<std::complex<double>> a,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes the metadata-only workspace formula for Tptrs.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
 * padding.
 * @return Matching formula plan or structural/overflow failure; no values
 * are read and no provider call or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryTptrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
    DenseBlasPackedMatrixView<const std::complex<double>> a,
    DenseBlasMatrixView<std::complex<double>> b);

/** @brief Solves op(A)*X=B through exact reference TPTRS.
 * @param provider Explicit pinned CPU provider, borrowed for this call.
 * @param triangle Stored upper/lower triangle; invalid values are rejected.
 * @param diagonal Explicit or implicit unit diagonal; invalid values fail.
 * @param operation N/T/C, retaining complex conjugate-transpose semantics.
 * @param a Borrowed packed coefficients; stored unit diagonal stays untouched.
 * @param b Borrowed n-by-nrhs RHS, independently laid out with unchanged
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
Tptrs(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
      DenseBlasDiagonal diagonal, DenseBlasTranspose operation,
      DenseBlasPackedMatrixView<const std::complex<double>> a,
      DenseBlasMatrixView<std::complex<double>> b,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIANGULAR_PACKED_H_
