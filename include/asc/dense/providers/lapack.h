#ifndef ASC_DENSE_PROVIDERS_LAPACK_H_
#define ASC_DENSE_PROVIDERS_LAPACK_H_

/** @file
 * @brief Explicit optional Reference-LAPACK LU operations; no implicit
 * fallback.
 *
 * Only the separately linked Dense-owned provider facet supplies these symbols.
 * Public declarations contain no foreign headers or integer/complex ABI types.
 * This initial slice supports host/pinned-host column-major descriptors only;
 * row-major operations return kUnsupported without touching numerical buffers.
 * All calls are synchronous and require serial CPU execution. Independent calls
 * with disjoint buffers/reports are reentrant. No allocation, transfer,
 * synchronization, hidden packing or global error-handler change is performed.
 *
 * Queries use exact checked formulas: GETRF/GETRS have no LWORK query. The
 * kInteger region contains min(m,n) / n provider-width pivot integer entries,
 * respectively; minimum and preferred are equal. Execution establishes those
 * trivial integer lifetimes in the supplied writable byte region. Other
 * numerical operands must already contain live objects of their declared type.
 * Plans bind routine, scalar, dimensions, leading dimensions, layout,
 * transpose, pivot increment/count and exact provider build/ABI, but not buffer
 * addresses. A copied query plan may be reused with disjoint equally shaped
 * buffers. Execution revalidates all requirements, aliases and pivot values
 * first.
 *
 * Raw signed Fortran INFO is preserved without LAPACKE argument-index shifting.
 * A structural failure leaves all numerical buffers unchanged, called_provider
 * false and INFO absent. GETRF positive INFO returns kNumerical with completed
 * raw LU/pivots, kSingular/kDocumentedPartial and zero-based first zero
 * diagonal. Negative INFO denotes a provider-contract defect and retains the
 * one-based foreign argument position; output is unusable. Success provides a
 * complete LU factor or solution. GETRS rechecks pivots and exact-zero U
 * diagonals before writing B, consumes without changing a same-build successful
 * square factor, and supports N/T/C. Empty operations complete without a
 * foreign call or INFO. Nonfinite arithmetic follows Reference LAPACK; success
 * is not a condition or finiteness guarantee. Callers preserve buffer lifetimes
 * and exclude mutation.
 */

#include <complex>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Copied explicit selection of this facet's attested reference build.
 *
 * The context is retained by value. No foreign handle, allocator, singleton or
 * registry is owned. Creation is allocation-free; it does not execute LAPACK.
 */
class ReferenceLapackProvider {
 public:
  /** @brief Selects the compiled provider for a serial CPU context.
   * @param context Retained serial execution context; no backend fallback.
   * @return A provider selection or kUnsupported for a nonserial context.
   */
  static ASC_DENSE_LAPACK_EXPORT Result<ReferenceLapackProvider> Create(
      ExecutionContext context);
  /** @brief Returns the retained context; valid while this selection is live.
   * @return Borrowed context reference, with no lifetime extension.
   */
  [[nodiscard]] const ExecutionContext& context() const noexcept {
    return context_;
  }
  /** @brief Returns the immutable copied source/build/integer ABI identity.
   * @return Borrowed identity reference, valid while this selection is live.
   */
  [[nodiscard]] const LapackProviderIdentity& identity() const noexcept {
    return identity_;
  }

 private:
  ReferenceLapackProvider(ExecutionContext context,
                          LapackProviderIdentity identity)
      : context_(std::move(context)), identity_(identity) {}
  ExecutionContext context_;
  LapackProviderIdentity identity_;
};

/** @brief Queries single real GETRF conversion capacities without mutation.
 * @param provider Explicit compiled reference provider.
 * @param matrix Checked column-major host matrix to be factored.
 * @param pivots Checked contiguous one-based output, size min(m,n).
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Queries single real GETRS conversion capacities without mutation.
 * @param provider Explicit same-build reference provider.
 * @param transpose Selects A, transpose(A) or conjugate-transpose(A).
 * @param factor Borrowed successful square LU and immutable raw pivots.
 * @param rhs Checked column-major host n-by-nrhs B/X, disjoint from factor.
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<float> factor, DenseBlasMatrixView<float> rhs);
/** @brief Factors a single real matrix through explicit Reference LAPACK.
 * @param provider Explicit reference provider; never inferred from context.
 * @param matrix In-place rectangular packed L/U, preserving padding.
 * @param pivots Contiguous min(m,n) signed one-based raw sequential row swaps.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural error, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status Getrf(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Solves a single real system with a same-build reusable LU.
 * @param provider Explicit reference provider matching factor provenance.
 * @param transpose Selects N/T/C; no factorization or factor mutation occurs.
 * @param factor Borrowed successful square factor with raw one-based pivots.
 * @param rhs In-place B/X, disjoint from factor, pivots and all workspace.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 */
ASC_DENSE_LAPACK_EXPORT Status Getrs(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose transpose,
                                     LapackLuFactorView<float> factor,
                                     DenseBlasMatrixView<float> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real GETRF conversion capacities without mutation.
 * @param provider Explicit compiled reference provider.
 * @param matrix Checked column-major host matrix to be factored.
 * @param pivots Checked contiguous one-based output, size min(m,n).
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Queries double real GETRS conversion capacities without mutation.
 * @param provider Explicit same-build reference provider.
 * @param transpose Selects A, transpose(A) or conjugate-transpose(A).
 * @param factor Borrowed successful square LU and immutable raw pivots.
 * @param rhs Checked column-major host n-by-nrhs B/X, disjoint from factor.
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<double> factor, DenseBlasMatrixView<double> rhs);
/** @brief Factors a double real matrix through explicit Reference LAPACK.
 * @param provider Explicit reference provider; never inferred from context.
 * @param matrix In-place rectangular packed L/U, preserving padding.
 * @param pivots Contiguous min(m,n) signed one-based raw sequential row swaps.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural error, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status Getrf(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Solves a double real system with a same-build reusable LU.
 * @param provider Explicit reference provider matching factor provenance.
 * @param transpose Selects N/T/C; no factorization or factor mutation occurs.
 * @param factor Borrowed successful square factor with raw one-based pivots.
 * @param rhs In-place B/X, disjoint from factor, pivots and all workspace.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 */
ASC_DENSE_LAPACK_EXPORT Status Getrs(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose transpose,
                                     LapackLuFactorView<double> factor,
                                     DenseBlasMatrixView<double> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single complex GETRF conversion capacities without mutation.
 * @param provider Explicit compiled reference provider.
 * @param matrix Checked column-major host matrix to be factored.
 * @param pivots Checked contiguous one-based output, size min(m,n).
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Queries single complex GETRS conversion capacities without mutation.
 * @param provider Explicit same-build reference provider.
 * @param transpose Selects A, transpose(A) or conjugate-transpose(A).
 * @param factor Borrowed successful square LU and immutable raw pivots.
 * @param rhs Checked column-major host n-by-nrhs B/X, disjoint from factor.
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Factors a single complex matrix through explicit Reference LAPACK.
 * @param provider Explicit reference provider; never inferred from context.
 * @param matrix In-place rectangular packed L/U, preserving padding.
 * @param pivots Contiguous min(m,n) signed one-based raw sequential row swaps.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural error, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getrf(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Solves a single complex system with a same-build reusable LU.
 * @param provider Explicit reference provider matching factor provenance.
 * @param transpose Selects N/T/C; no factorization or factor mutation occurs.
 * @param factor Borrowed successful square factor with raw one-based pivots.
 * @param rhs In-place B/X, disjoint from factor, pivots and all workspace.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getrs(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      LapackLuFactorView<std::complex<float>> factor,
      DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex GETRF conversion capacities without mutation.
 * @param provider Explicit compiled reference provider.
 * @param matrix Checked column-major host matrix to be factored.
 * @param pivots Checked contiguous one-based output, size min(m,n).
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrfWorkspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);
/** @brief Queries double complex GETRS conversion capacities without mutation.
 * @param provider Explicit same-build reference provider.
 * @param transpose Selects A, transpose(A) or conjugate-transpose(A).
 * @param factor Borrowed successful square LU and immutable raw pivots.
 * @param rhs Checked column-major host n-by-nrhs B/X, disjoint from factor.
 * @return Exact minimum/preferred plan or structural failure; no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    LapackLuFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Factors a double complex matrix through explicit Reference LAPACK.
 * @param provider Explicit reference provider; never inferred from context.
 * @param matrix In-place rectangular packed L/U, preserving padding.
 * @param pivots Contiguous min(m,n) signed one-based raw sequential row swaps.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural error, or kNumerical for exact singularity.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getrf(const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Solves a double complex system with a same-build reusable LU.
 * @param provider Explicit reference provider matching factor provenance.
 * @param transpose Selects N/T/C; no factorization or factor mutation occurs.
 * @param factor Borrowed successful square factor with raw one-based pivots.
 * @param rhs In-place B/X, disjoint from factor, pivots and all workspace.
 * @param plan Unmodified query result matching all current metadata.
 * @param workspace Caller-owned disjoint conversion storage; see file contract.
 * @param report Mandatory failure-surviving diagnostics; reset before
 * preflight.
 * @return OK, structural failure, or kNumerical for an exact-zero U diagonal.
 */
ASC_DENSE_LAPACK_EXPORT Status
Getrs(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      LapackLuFactorView<std::complex<double>> factor,
      DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_H_
