#ifndef ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_H_
#define ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_H_

/** @file
 * @brief Explicit reference general-tridiagonal LU factors and reused solves.
 *
 * DL/D/DU are separate contiguous arrays; GTTRF additionally outputs DU2,
 * the second superdiagonal of U. A=L*U, where L is the ordered product of
 * adjacent permutations and unit lower bidiagonal eliminations, not a plain
 * stored lower triangle. No band/full conversion or densification occurs.
 *
 * All calls are synchronous serial CPU operations of the selected provider.
 * Every referenced operand and nonempty workspace must be context-accessible.
 * Caller objects, metadata and backing storage remain live throughout use;
 * independently concurrent calls need disjoint writable storage and reports.
 * No allocation, transfer, synchronization, precision/provider fallback or
 * global error-handler change occurs. Foreign-integer lifetimes are started
 * in caller byte workspace; other numerical objects must already be alive.
 *
 * Queries inspect metadata only, not factor/pivot entries, and make no foreign
 * call. Plans bind source routine, scalar, dimensions, original and effective
 * strides, layouts, transpose and provider ABI/build, not buffer addresses.
 * Active GTTRF/GTTRS need n actual signed provider integers in
 * kInteger. GTTRS row-major B additionally needs n*nrhs live scalar
 * kLayoutConversion entries. Column-major B needs no packing; unused original
 * strides are normalized only at the private ABI boundary and stay in the key.
 * Structural validation precedes all numerical/workspace writes and calls.
 *
 * Reports have exact scalar-prefixed routine identity and leave factor_family
 * absent: the generic enum does not encode tridiagonal LU. These nominal
 * pivot/factor types prevent reinterpretation as GETRF or block pivots.
 * Empty operations complete locally with called_provider=false and no INFO.
 * Other successful returns have raw INFO=0; positive GTTRF INFO is the first
 * exactly zero U diagonal with completed raw factors/pivots, not a usable
 * successful factor. Unexpected INFO is a provider defect. Success does not
 * promise conditioning or finite arithmetic. No synthetic factor report is
 * accepted. Any detected alias between metadata objects or between metadata
 * and an operand/workspace region leaves the report untouched. After metadata
 * alias checks pass, the report is reset before the remaining preflight;
 * numerical outputs remain unchanged on structural failure.
 */

#include <complex>
#include <cstddef>
#include <span>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Nominal borrowed GTTRF adjacent one-based sequential-pivot payload.
 *
 * Values are ASC signed index_t, not foreign integers or a final permutation.
 * At zero-based i<n-1 the raw entry is i+1 or i+2; the last is n. Construction
 * checks storage only; every active consumer validates values after context
 * admission, allowing metadata-only queries without reading numerical input.
 * Owners and immutable entries must outlive every use; mutation invalidates
 * any earlier value validation. No ownership, allocation or transfer occurs.
 */
class ReferenceTridiagonalPivotView {
 public:
  /** @brief Checks contiguous CPU metadata without reading pivot entries.
   * @param values Borrowed signed ASC entries, increment one, host/pinned.
   * @return Nominal view or shape/placement failure with all bytes unchanged.
   */
  static ASC_DENSE_LAPACK_EXPORT Result<ReferenceTridiagonalPivotView> Create(
      DenseBlasVectorView<const index_t> values);
  /** @brief Returns the borrowed signed one-based adjacent pivot entries.
   * @return Contiguous span, valid only for the underlying owner's lifetime.
   */
  [[nodiscard]] std::span<const index_t> values() const noexcept {
    return {values_.data(), static_cast<std::size_t>(values_.size())};
  }
  /** @brief Returns the exact reachable bytes for context and alias checks.
   * @return Borrowed CPU memory metadata; no lifetime is extended.
   */
  [[nodiscard]] ConstMemoryView reachable_storage() const noexcept {
    return values_.reachable_storage();
  }

 private:
  explicit ReferenceTridiagonalPivotView(
      DenseBlasVectorView<const index_t> values)
      : values_(values) {}
  DenseBlasVectorView<const index_t> values_;
};

/** @brief Borrows same-call successful GTTRF factors and adjacent pivots.
 *
 * Available for float, double and their standard complex counterparts.
 * The caller guarantees common origin of report, DL/D/DU/DU2 and pivots.
 * A report cannot prove buffer contents; subsequent mutation or expiration
 * invalidates this view. Copies do not extend storage/provider lifetimes.
 * Independent immutable reuse with distinct RHS/workspace/report is allowed.
 */
template <DenseBlasScalar Element>
class ReferenceTridiagonalLuFactorView {
 public:
  /** @brief Checks provenance, context, aliases, adjacent pivots and nonzero D.
   * @param provider The explicit provider from the originating GTTRF call.
   * @param factors Same-call immutable primary factors and separate DU2.
   * @param pivots Same-call nominal adjacent pivots, exact matrix order.
   * @param report Actual successful complete same-scalar GTTRF report. Empty
   * order permits the documented local completion with no raw INFO.
   * @return Borrowed factor or structural/state/numerical error, no writes,
   * allocation or foreign call. GTSV/GTSVX or fabricated TRF reports are not
   * accepted as an originating factorization. No finiteness guarantee follows.
   */
  static ASC_DENSE_LAPACK_EXPORT Result<ReferenceTridiagonalLuFactorView>
  Create(const ReferenceLapackProvider& provider,
         LapackTridiagonalLuStorage<const Element> factors,
         ReferenceTridiagonalPivotView pivots, const LapackReport& report);
  /** @brief Returns borrowed raw primary factors and explicit DU2.
   * @return Immutable storage retaining all four separate buffer lifetimes.
   */
  [[nodiscard]] LapackTridiagonalLuStorage<const Element> factors()
      const noexcept {
    return factors_;
  }
  /** @brief Returns borrowed adjacent raw pivots.
   * @return Nominal tridiagonal payload, never a generic permutation.
   */
  [[nodiscard]] ReferenceTridiagonalPivotView pivots() const noexcept {
    return pivots_;
  }
  /** @brief Returns copied originating provider/build provenance.
   * @return Reference valid for this metadata object's lifetime.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }

 private:
  ReferenceTridiagonalLuFactorView(
      LapackTridiagonalLuStorage<const Element> factors,
      ReferenceTridiagonalPivotView pivots, LapackProviderIdentity provider)
      : factors_(factors), pivots_(pivots), provider_(provider) {}
  LapackTridiagonalLuStorage<const Element> factors_;
  ReferenceTridiagonalPivotView pivots_;
  LapackProviderIdentity provider_;
};

/** @brief Computes exact single real GTTRF workspace capacities.
 * @param provider Explicit compiled reference provider.
 * @param factors Mutable DL/D/DU inputs and output-only DU2, all unread.
 * @param pivots Mutable contiguous index_t output, exact order, unread.
 * @return Checked metadata-bound plan or structural failure without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<float> factors,
    DenseBlasVectorView<index_t> pivots);
/** @brief Computes the actual single real tridiagonal LU with adjacent swaps.
 * @param provider Explicit checked reference selection, no fallback.
 * @param factors DL/D/DU overwritten with factors; separate DU2 is output-only.
 * @param pivots Exact-order contiguous ASC signed one-based pivot output.
 * @param plan Unmodified matching QueryGttrfWorkspace result.
 * @param workspace Disjoint caller kInteger bytes, no hidden buffers.
 * @param report Mandatory failure-surviving raw INFO and output validity.
 * @return OK, structural/provider failure, or kNumerical for exact singularity.
 * Positive INFO retains completed raw factors and valid pivots; see file-level
 * failure/lifetime/concurrency contract. O(n), no allocation or transfer.
 */
ASC_DENSE_LAPACK_EXPORT Status Gttrf(const ReferenceLapackProvider& provider,
                                     LapackTridiagonalLuStorage<float> factors,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Computes exact single real reused-solve workspace capacities.
 * @param provider Same explicit provider as the borrowed successful factor.
 * @param transpose N/T/C, with distinct complex conjugate-transpose semantics.
 * @param factor Borrowed successful GTTRF metadata; no values are read.
 * @param rhs Mutable n-by-nrhs B in either layout, numerical entries unread.
 * @return Matching fixed-formula plan or structural/provenance failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<float> factor,
    DenseBlasMatrixView<float> rhs);
/** @brief Solves op(A)*X=B using immutable single real GTTRF factors.
 * @param provider Same explicit reference provider/build as the factor.
 * @param transpose N/T/C; real C equals T, complex C conjugates.
 * @param factor Immutable factors/pivots revalidated before any RHS writes.
 * @param rhs Independent-layout n-by-nrhs B overwritten with X, padding kept.
 * @param plan Unmodified matching QueryGttrsWorkspace result.
 * @param workspace Caller native-pivot bytes and explicit row-major RHS
 * packing.
 * @param report Mandatory failure-surviving diagnostics, no fabricated INFO.
 * @return OK, structural/provider failure or exact-zero diagonal rejection
 * before foreign entry. Empty RHS calls do not read factors/pivots. O(n*nrhs),
 * no factorization, allocation, transfer or implicit densification.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gttrs(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      ReferenceTridiagonalLuFactorView<float> factor,
      DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact double real GTTRF workspace capacities.
 * @param provider Explicit compiled reference provider.
 * @param factors Mutable DL/D/DU inputs and output-only DU2, all unread.
 * @param pivots Mutable contiguous index_t output, exact order, unread.
 * @return Checked metadata-bound plan or structural failure without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<double> factors,
    DenseBlasVectorView<index_t> pivots);
/** @brief Computes the actual double real tridiagonal LU with adjacent swaps.
 * @param provider Explicit checked reference selection, no fallback.
 * @param factors DL/D/DU overwritten with factors; separate DU2 is output-only.
 * @param pivots Exact-order contiguous ASC signed one-based pivot output.
 * @param plan Unmodified matching QueryGttrfWorkspace result.
 * @param workspace Disjoint caller kInteger bytes, no hidden buffers.
 * @param report Mandatory failure-surviving raw INFO and output validity.
 * @return OK, structural/provider failure, or kNumerical for exact singularity.
 * Positive INFO retains completed raw factors and valid pivots; see file-level
 * failure/lifetime/concurrency contract. O(n), no allocation or transfer.
 */
ASC_DENSE_LAPACK_EXPORT Status Gttrf(const ReferenceLapackProvider& provider,
                                     LapackTridiagonalLuStorage<double> factors,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Computes exact double real reused-solve workspace capacities.
 * @param provider Same explicit provider as the borrowed successful factor.
 * @param transpose N/T/C, with distinct complex conjugate-transpose semantics.
 * @param factor Borrowed successful GTTRF metadata; no values are read.
 * @param rhs Mutable n-by-nrhs B in either layout, numerical entries unread.
 * @return Matching fixed-formula plan or structural/provenance failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<double> factor,
    DenseBlasMatrixView<double> rhs);
/** @brief Solves op(A)*X=B using immutable double real GTTRF factors.
 * @param provider Same explicit reference provider/build as the factor.
 * @param transpose N/T/C; real C equals T, complex C conjugates.
 * @param factor Immutable factors/pivots revalidated before any RHS writes.
 * @param rhs Independent-layout n-by-nrhs B overwritten with X, padding kept.
 * @param plan Unmodified matching QueryGttrsWorkspace result.
 * @param workspace Caller native-pivot bytes and explicit row-major RHS
 * packing.
 * @param report Mandatory failure-surviving diagnostics, no fabricated INFO.
 * @return OK, structural/provider failure or exact-zero diagonal rejection
 * before foreign entry. Empty RHS calls do not read factors/pivots. O(n*nrhs),
 * no factorization, allocation, transfer or implicit densification.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gttrs(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      ReferenceTridiagonalLuFactorView<double> factor,
      DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Computes exact single complex GTTRF workspace capacities.
 * @param provider Explicit compiled reference provider.
 * @param factors Mutable DL/D/DU inputs and output-only DU2, all unread.
 * @param pivots Mutable contiguous index_t output, exact order, unread.
 * @return Checked metadata-bound plan or structural failure without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<std::complex<float>> factors,
    DenseBlasVectorView<index_t> pivots);
/** @brief Computes the actual single complex tridiagonal LU with adjacent
 * swaps.
 * @param provider Explicit checked reference selection, no fallback.
 * @param factors DL/D/DU overwritten with factors; separate DU2 is output-only.
 * @param pivots Exact-order contiguous ASC signed one-based pivot output.
 * @param plan Unmodified matching QueryGttrfWorkspace result.
 * @param workspace Disjoint caller kInteger bytes, no hidden buffers.
 * @param report Mandatory failure-surviving raw INFO and output validity.
 * @return OK, structural/provider failure, or kNumerical for exact singularity.
 * Positive INFO retains completed raw factors and valid pivots; see file-level
 * failure/lifetime/concurrency contract. O(n), no allocation or transfer.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gttrf(const ReferenceLapackProvider& provider,
      LapackTridiagonalLuStorage<std::complex<float>> factors,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Computes exact single complex reused-solve workspace capacities.
 * @param provider Same explicit provider as the borrowed successful factor.
 * @param transpose N/T/C, with distinct complex conjugate-transpose semantics.
 * @param factor Borrowed successful GTTRF metadata; no values are read.
 * @param rhs Mutable n-by-nrhs B in either layout, numerical entries unread.
 * @return Matching fixed-formula plan or structural/provenance failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Solves op(A)*X=B using immutable single complex GTTRF factors.
 * @param provider Same explicit reference provider/build as the factor.
 * @param transpose N/T/C; real C equals T, complex C conjugates.
 * @param factor Immutable factors/pivots revalidated before any RHS writes.
 * @param rhs Independent-layout n-by-nrhs B overwritten with X, padding kept.
 * @param plan Unmodified matching QueryGttrsWorkspace result.
 * @param workspace Caller native-pivot bytes and explicit row-major RHS
 * packing.
 * @param report Mandatory failure-surviving diagnostics, no fabricated INFO.
 * @return OK, structural/provider failure or exact-zero diagonal rejection
 * before foreign entry. Empty RHS calls do not read factors/pivots. O(n*nrhs),
 * no factorization, allocation, transfer or implicit densification.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gttrs(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      ReferenceTridiagonalLuFactorView<std::complex<float>> factor,
      DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Computes exact double complex GTTRF workspace capacities.
 * @param provider Explicit compiled reference provider.
 * @param factors Mutable DL/D/DU inputs and output-only DU2, all unread.
 * @param pivots Mutable contiguous index_t output, exact order, unread.
 * @return Checked metadata-bound plan or structural failure without writes.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackTridiagonalLuStorage<std::complex<double>> factors,
    DenseBlasVectorView<index_t> pivots);
/** @brief Computes the actual double complex tridiagonal LU with adjacent
 * swaps.
 * @param provider Explicit checked reference selection, no fallback.
 * @param factors DL/D/DU overwritten with factors; separate DU2 is output-only.
 * @param pivots Exact-order contiguous ASC signed one-based pivot output.
 * @param plan Unmodified matching QueryGttrfWorkspace result.
 * @param workspace Disjoint caller kInteger bytes, no hidden buffers.
 * @param report Mandatory failure-surviving raw INFO and output validity.
 * @return OK, structural/provider failure, or kNumerical for exact singularity.
 * Positive INFO retains completed raw factors and valid pivots; see file-level
 * failure/lifetime/concurrency contract. O(n), no allocation or transfer.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gttrf(const ReferenceLapackProvider& provider,
      LapackTridiagonalLuStorage<std::complex<double>> factors,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);
/** @brief Computes exact double complex reused-solve workspace capacities.
 * @param provider Same explicit provider as the borrowed successful factor.
 * @param transpose N/T/C, with distinct complex conjugate-transpose semantics.
 * @param factor Borrowed successful GTTRF metadata; no values are read.
 * @param rhs Mutable n-by-nrhs B in either layout, numerical entries unread.
 * @return Matching fixed-formula plan or structural/provenance failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGttrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
    ReferenceTridiagonalLuFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Solves op(A)*X=B using immutable double complex GTTRF factors.
 * @param provider Same explicit reference provider/build as the factor.
 * @param transpose N/T/C; real C equals T, complex C conjugates.
 * @param factor Immutable factors/pivots revalidated before any RHS writes.
 * @param rhs Independent-layout n-by-nrhs B overwritten with X, padding kept.
 * @param plan Unmodified matching QueryGttrsWorkspace result.
 * @param workspace Caller native-pivot bytes and explicit row-major RHS
 * packing.
 * @param report Mandatory failure-surviving diagnostics, no fabricated INFO.
 * @return OK, structural/provider failure or exact-zero diagonal rejection
 * before foreign entry. Empty RHS calls do not read factors/pivots. O(n*nrhs),
 * no factorization, allocation, transfer or implicit densification.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gttrs(const ReferenceLapackProvider& provider, DenseBlasTranspose transpose,
      ReferenceTridiagonalLuFactorView<std::complex<double>> factor,
      DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_TRIDIAGONAL_H_
