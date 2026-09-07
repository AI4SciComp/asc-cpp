#ifndef ASC_DENSE_LAPACK_QR_H_
#define ASC_DENSE_LAPACK_QR_H_

/** @file
 * @brief Native packed Householder QR with explicit Q formation/application.
 *
 * GEQRF stores upper trapezoidal R and min(m,n) elementary reflectors in A;
 * v(i)=1 is implicit, its tail is below the diagonal, and tau is separate.
 * Q=H(0)*...*H(k-1), H(i)=I-tau(i)*v(i)*v(i)^H. Complex reflectors use the
 * real-beta GEQRF convention: H(i)^H annihilates the column tail; H itself
 * need not be Hermitian. Real zero tails, or complex zero tails with real
 * alpha, produce tau=0 and preserve alpha. No positive-diagonal or numerical
 * rank certificate is implied; rank-deficient matrices can factor successfully.
 *
 * Every operation uses existing row/column-major BLAS descriptors directly,
 * preserving padding. Caller-owned contiguous scalar scratch is mandatory
 * when a nonempty reflector operation needs it; required lengths are stated
 * below and checked against actual operands on every call. Scratch is not a
 * cached query plan. All simultaneously live reachable operand spans and the
 * complete supplied scratch span are disjoint.
 * No allocation, hidden packing, transfer, synchronization, or foreign fallback
 * occurs. Only serial CPU contexts and host buffers are supported; the existing
 * serial context cannot access pinned/device/managed memory, so those
 * placements are rejected before numerical access. Owners and already-live
 * scalar objects outlive each synchronous call; callers exclude concurrent
 * mutation. Independent disjoint calls may run concurrently.
 *
 * Reports are initialized before ordinary validation; an overlapping report is
 * rejected without overwriting its aliased operand. Structural failures leave
 * all numerical buffers unchanged. Nonfinite referenced numerical input returns
 * kNumerical with kNotRun/kUnchanged before writes. A nonrepresentable computed
 * norm/update returns kNumerical with kPartialResult/kUnusable and the
 * zero-based reflector diagnostic_index. Such raw partial buffers are not
 * reusable factors. Native calls never set called_provider or foreign
 * native_info. Successful completion sets kSuccess/kComplete; it does not
 * certify conditioning or rank.
 *
 * FormHouseholderQ and ApplyHouseholderQ are separate-output/native convenience
 * operations over all reflectors of a successful factor. Their implementation
 * is not complete exact ORG, UNG, ORM or UNM provider-family coverage.
 */
#include <complex>

#include "asc/core/execution.h"
#include "asc/core/status.h"
#include "asc/dense/blas.h"
#include "asc/dense/export.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"

namespace asc {

/** @brief Factors a rectangular single-real matrix with native Householders.
 * @param context Explicit serial CPU execution context.
 * @param matrix Mutable m-by-n A, replaced by packed reflectors and R.
 * @param tau Mutable contiguous scalar output of exact length min(m,n).
 * @param scratch Disjoint contiguous caller scalars, length at least n for
 * nonempty A, otherwise zero; contents may change after execution begins.
 * @param report Mandatory owned diagnostics, subject to this file's contract.
 * @return OK or structural/numerical failure. O(m*n*min(m,n)) arithmetic;
 * no allocation, transfer or implicit provider selection.
 */
ASC_DENSE_EXPORT Status Geqrf(const ExecutionContext& context,
                              DenseBlasMatrixView<float> matrix,
                              DenseBlasVectorView<float> tau,
                              DenseBlasVectorView<float> scratch,
                              LapackReport& report);
/** @brief Factors a rectangular double-real matrix with native Householders.
 * @param context Explicit serial CPU execution context.
 * @param matrix Mutable m-by-n A, replaced by packed reflectors and R.
 * @param tau Mutable contiguous scalar output of exact length min(m,n).
 * @param scratch Disjoint contiguous scalars, at least n for nonempty A,
 * otherwise zero; contents may change after execution begins.
 * @param report Mandatory owned diagnostics, subject to this file's contract.
 * @return OK or structural/numerical failure. O(m*n*min(m,n)) arithmetic;
 * no allocation, transfer or implicit provider selection.
 */
ASC_DENSE_EXPORT Status Geqrf(const ExecutionContext& context,
                              DenseBlasMatrixView<double> matrix,
                              DenseBlasVectorView<double> tau,
                              DenseBlasVectorView<double> scratch,
                              LapackReport& report);
/** @brief Factors a rectangular single-complex matrix with native
 * Householders.
 * @param context Explicit serial CPU execution context.
 * @param matrix Mutable m-by-n A, replaced by packed reflectors and R.
 * @param tau Mutable contiguous complex output of exact length min(m,n).
 * @param scratch Disjoint contiguous complex scalars, at least n for nonempty
 * A, otherwise zero; contents may change after execution begins.
 * @param report Mandatory owned diagnostics, subject to this file's contract.
 * @return OK or structural/numerical failure. O(m*n*min(m,n)) arithmetic;
 * no allocation, transfer or implicit provider selection.
 */
ASC_DENSE_EXPORT Status Geqrf(const ExecutionContext& context,
                              DenseBlasMatrixView<std::complex<float>> matrix,
                              DenseBlasVectorView<std::complex<float>> tau,
                              DenseBlasVectorView<std::complex<float>> scratch,
                              LapackReport& report);
/** @brief Factors a rectangular double-complex matrix with native
 * Householders.
 * @param context Explicit serial CPU execution context.
 * @param matrix Mutable m-by-n A, replaced by packed reflectors and R.
 * @param tau Mutable contiguous complex output of exact length min(m,n).
 * @param scratch Disjoint contiguous complex scalars, at least n for nonempty
 * A, otherwise zero; contents may change after execution begins.
 * @param report Mandatory owned diagnostics, subject to this file's contract.
 * @return OK or structural/numerical failure. O(m*n*min(m,n)) arithmetic;
 * no allocation, transfer or implicit provider selection.
 */
ASC_DENSE_EXPORT Status Geqrf(const ExecutionContext& context,
                              DenseBlasMatrixView<std::complex<double>> matrix,
                              DenseBlasVectorView<std::complex<double>> tau,
                              DenseBlasVectorView<std::complex<double>> scratch,
                              LapackReport& report);

/** @brief Forms explicit full/economy single-real Q without changing its
 * factor.
 * @param context Explicit serial CPU context.
 * @param factor Successful unmodified native GEQRF factor of shape m-by-n.
 * @param q Disjoint output of shape m-by-m or m-by-min(m,n); all entries
 * written.
 * @param scratch Disjoint contiguous scalars, at least q.columns() if both Q
 * and the reflector sequence are nonempty, otherwise zero.
 * @param report Mandatory diagnostics; no foreign INFO or rank is fabricated.
 * @return OK or structural/numerical failure. O(m*q.columns()*min(m,n)) work
 * plus explicit identity initialization; no allocation/transfer/fallback.
 */
ASC_DENSE_EXPORT Status FormHouseholderQ(
    const ExecutionContext& context,
    LapackHouseholderQrFactorView<float> factor, DenseBlasMatrixView<float> q,
    DenseBlasVectorView<float> scratch, LapackReport& report);
/** @brief Forms explicit full/economy double-real Q without changing its
 * factor.
 * @param context Explicit serial CPU context.
 * @param factor Successful unmodified native GEQRF factor of shape m-by-n.
 * @param q Disjoint output of shape m-by-m or m-by-min(m,n); all entries
 * written.
 * @param scratch Disjoint contiguous scalars, at least q.columns() if both Q
 * and the reflector sequence are nonempty, otherwise zero.
 * @param report Mandatory diagnostics; no foreign INFO or rank is fabricated.
 * @return OK or structural/numerical failure. O(m*q.columns()*min(m,n)) work
 * plus identity initialization; no allocation/transfer/fallback.
 */
ASC_DENSE_EXPORT Status FormHouseholderQ(
    const ExecutionContext& context,
    LapackHouseholderQrFactorView<double> factor, DenseBlasMatrixView<double> q,
    DenseBlasVectorView<double> scratch, LapackReport& report);
/** @brief Forms full/economy single-complex unitary Q from preserved factors.
 * @param context Explicit serial CPU context.
 * @param factor Successful unmodified native GEQRF factor of shape m-by-n.
 * @param q Disjoint output of shape m-by-m or m-by-min(m,n); all entries
 * written.
 * @param scratch Disjoint contiguous complex scalars, at least q.columns() if
 * Q and the reflector sequence are nonempty, otherwise zero.
 * @param report Mandatory diagnostics; no foreign INFO or rank is fabricated.
 * @return OK or structural/numerical failure. O(m*q.columns()*min(m,n)) work
 * plus identity initialization; no allocation/transfer/fallback.
 */
ASC_DENSE_EXPORT Status FormHouseholderQ(
    const ExecutionContext& context,
    LapackHouseholderQrFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> q,
    DenseBlasVectorView<std::complex<float>> scratch, LapackReport& report);
/** @brief Forms full/economy double-complex unitary Q from preserved factors.
 * @param context Explicit serial CPU context.
 * @param factor Successful unmodified native GEQRF factor of shape m-by-n.
 * @param q Disjoint output of shape m-by-m or m-by-min(m,n); all entries
 * written.
 * @param scratch Disjoint contiguous complex scalars, at least q.columns() if
 * Q and the reflector sequence are nonempty, otherwise zero.
 * @param report Mandatory diagnostics; no foreign INFO or rank is fabricated.
 * @return OK or structural/numerical failure. O(m*q.columns()*min(m,n)) work
 * plus identity initialization; no allocation/transfer/fallback.
 */
ASC_DENSE_EXPORT Status FormHouseholderQ(
    const ExecutionContext& context,
    LapackHouseholderQrFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> q,
    DenseBlasVectorView<std::complex<double>> scratch, LapackReport& report);

/** @brief Applies preserved single-real Q or Q^T from the left or right.
 * @param context Explicit serial CPU execution context.
 * @param side Selects op(Q)*C or C*op(Q), with full Q order factor.rows().
 * @param transpose None or transpose; other modes are rejected before writes.
 * @param factor Successful native packed QR; R/implicit diagonal are not
 * read.
 * @param matrix Mutable C with the selected dimension matching Q order.
 * @param scratch Disjoint contiguous scalars, at least C.columns() for left
 * or C.rows() for right, or zero if C/reflector sequence is empty.
 * @param report Mandatory diagnostics; factors remain unchanged on all
 * outcomes.
 * @return OK or structural/numerical failure. O(C.rows()*C.columns()*k) work,
 * no formation of Q, allocation, transfer or implicit provider fallback.
 */
ASC_DENSE_EXPORT Status ApplyHouseholderQ(
    const ExecutionContext& context, DenseBlasSide side,
    DenseBlasTranspose transpose, LapackHouseholderQrFactorView<float> factor,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<float> scratch,
    LapackReport& report);
/** @brief Applies preserved double-real Q or Q^T from the left or right.
 * @param context Explicit serial CPU execution context.
 * @param side Selects op(Q)*C or C*op(Q), with full Q order factor.rows().
 * @param transpose None or transpose; other modes are rejected before writes.
 * @param factor Successful native packed QR; R/implicit diagonal are not
 * read.
 * @param matrix Mutable C with the selected dimension matching Q order.
 * @param scratch Disjoint contiguous scalars, at least C.columns() for left
 * or C.rows() for right, or zero if C/reflector sequence is empty.
 * @param report Mandatory diagnostics; factors remain unchanged on all
 * outcomes.
 * @return OK or structural/numerical failure. O(C.rows()*C.columns()*k) work,
 * no formation of Q, allocation, transfer or implicit provider fallback.
 */
ASC_DENSE_EXPORT Status ApplyHouseholderQ(
    const ExecutionContext& context, DenseBlasSide side,
    DenseBlasTranspose transpose, LapackHouseholderQrFactorView<double> factor,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<double> scratch,
    LapackReport& report);
/** @brief Applies preserved single-complex Q or Q^H from the left or right.
 * @param context Explicit serial CPU execution context.
 * @param side Selects op(Q)*C or C*op(Q), with full Q order factor.rows().
 * @param transpose None or conjugate transpose; ordinary transpose is
 * rejected.
 * @param factor Successful native packed QR; R/implicit diagonal are not
 * read.
 * @param matrix Mutable C with the selected dimension matching Q order.
 * @param scratch Disjoint contiguous complex scalars, at least C.columns()
 * for left or C.rows() for right, or zero if C/reflector sequence is empty.
 * @param report Mandatory diagnostics; factors remain unchanged on all
 * outcomes.
 * @return OK or structural/numerical failure. O(C.rows()*C.columns()*k) work,
 * no formation of Q, allocation, transfer or implicit provider fallback.
 */
ASC_DENSE_EXPORT Status ApplyHouseholderQ(
    const ExecutionContext& context, DenseBlasSide side,
    DenseBlasTranspose transpose,
    LapackHouseholderQrFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<std::complex<float>> scratch, LapackReport& report);
/** @brief Applies preserved double-complex Q or Q^H from the left or right.
 * @param context Explicit serial CPU execution context.
 * @param side Selects op(Q)*C or C*op(Q), with full Q order factor.rows().
 * @param transpose None or conjugate transpose; ordinary transpose is
 * rejected.
 * @param factor Successful native packed QR; R/implicit diagonal are not
 * read.
 * @param matrix Mutable C with the selected dimension matching Q order.
 * @param scratch Disjoint contiguous complex scalars, at least C.columns()
 * for left or C.rows() for right, or zero if C/reflector sequence is empty.
 * @param report Mandatory diagnostics; factors remain unchanged on all
 * outcomes.
 * @return OK or structural/numerical failure. O(C.rows()*C.columns()*k) work,
 * no formation of Q, allocation, transfer or implicit provider fallback.
 */
ASC_DENSE_EXPORT Status ApplyHouseholderQ(
    const ExecutionContext& context, DenseBlasSide side,
    DenseBlasTranspose transpose,
    LapackHouseholderQrFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<std::complex<double>> scratch, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_QR_H_
