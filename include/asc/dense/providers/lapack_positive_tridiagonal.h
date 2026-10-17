#ifndef ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_H_
#define ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_H_

/** @file
 * @brief Explicit Reference PTTRF/PTTRS with real D and real/complex E.
 *
 * PTTRF overwrites the existing lower tridiagonal descriptor with D and the
 * unit lower bidiagonal L in A=L*D*L^H (transpose for real scalars). It has no
 * native UPLO or workspace argument. Both arrays are contiguous; no dense
 * conversion occurs. PTTRS consumes immutable factors and overwrites B with X.
 * Upper raw factors represent A=U^H*D*U: their E must be the conjugate of the
 * corresponding lower E for the same complex matrix. No implicit conjugation
 * of arbitrary raw factors occurs. The explicit raw factory records
 * orientation.
 *
 * Queries inspect metadata only and never enter the provider. Plans bind the
 * actual scalar routine, order, RHS shape/layout/strides, factor orientation
 * and provider identity, not buffer addresses. Native scratch is zero. Active
 * row-major solves need n*nrhs live T objects in kLayoutConversion; direct
 * column-major solves and factorizations need no workspace. All metadata,
 * placement, overlap and workspace validation precedes numeric writes/entry.
 * Metadata aliases preserve report; subsequent preflight resets report while
 * preserving numeric and scratch bytes. All callers keep descriptors, buffers,
 * provider, plans and reports alive. No operation allocation or transfer
 * occurs.
 *
 * Empty factorization and N=0 or NRHS=0 solves complete locally, with absent
 * INFO and no numeric reads. Nonempty factorization preserves exact native
 * INFO: 1..N means a nonpositive leading minor. INFO<N leaves an incomplete
 * factor; INFO=N leaves a completed factorization with nonpositive last D.
 * Both return kNumerical/kNotPositiveDefinite and documented partial output,
 * never a successful factor certificate. Negative/impossible/unwritten INFO
 * is a provider defect; direct native writes cannot be rolled back.
 *
 * Native INFO=0 alone does not prove finite arithmetic. Active solves check
 * finite positive D and finite E before packing/entry, but do not infer that
 * PTTRS diagnoses bad supplied factors. Nonfinite solution publication returns
 * kNumerical/kAccuracyWarning with documented partial output and raw INFO=0.
 * No diagnostic clamp, scaling, alternate provider or algorithm is introduced.
 * factor_family stays absent: PT factors are not square-root Cholesky factors.
 * Factorization costs O(n); solving costs O(n*nrhs), with O(n) validation.
 * Independent calls and immutable factor/plan reuse are reentrant with disjoint
 * RHS, workspace and reports. Link the optional admitted ASC::dense_lapack
 * facet.
 */

#include <complex>
#include <utility>

#include "asc/core/extents.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/array.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Borrows nominal real-diagonal unit-bidiagonal factors.
 *
 * This is not the input-matrix descriptor or a generic Cholesky factor.
 * Copies retain provider identity and orientation, not storage ownership.
 * Owners must outlive use; mutation invalidates previous numerical validation.
 */
template <DenseBlasScalar Element>
class ReferencePositiveDefiniteTridiagonalFactorView {
 public:
  /** @brief Borrows finite positive factors from the same successful PTTRF.
   * @param provider Same explicit provider as the producing call.
   * @param factors Actual immutable D/lower E produced by that call.
   * @param report Actual same-scalar successful complete PTTRF report.
   * @return Lower factor or structural/state/numerical failure, without writes
   * or native entry. Caller guarantees common origin; report cannot prove it.
   */
  static ASC_DENSE_LAPACK_EXPORT
      Result<ReferencePositiveDefiniteTridiagonalFactorView>
      Create(const ReferenceLapackProvider& provider,
             LapackPositiveDefiniteTridiagonalView<const Element> factors,
             const LapackReport& report);

  /** @brief Borrows explicitly supplied raw D/E without claiming a prior call.
   * @param provider Explicit provider for subsequent queries/execution.
   * @param triangle Physical unit-bidiagonal storage, upper or lower.
   * @param diagonal Real D, length n; active solves require finite positive D.
   * @param off_diagonal E, length max(n-1,0); active solves require finite E.
   * @return Metadata-validated raw view; no entries are read and no positive-
   * definiteness or successful-factorization certificate is manufactured.
   */
  static ASC_DENSE_LAPACK_EXPORT
      Result<ReferencePositiveDefiniteTridiagonalFactorView>
      FromRaw(const ReferenceLapackProvider& provider,
              DenseBlasTriangle triangle,
              DenseBlasVectorView<const DenseBlasRealType<Element>> diagonal,
              DenseBlasVectorView<const Element> off_diagonal);

  /** @brief Returns borrowed real D; lifetime is not extended.
   * @return Immutable contiguous diagonal metadata.
   */
  [[nodiscard]] auto diagonal() const noexcept { return diagonal_; }
  /** @brief Returns borrowed E in the recorded physical orientation.
   * @return Immutable contiguous off-diagonal metadata.
   */
  [[nodiscard]] auto off_diagonal() const noexcept { return off_diagonal_; }
  /** @brief Returns the physical upper/lower unit-bidiagonal orientation.
   * @return Upper denotes U^H*D*U; lower denotes L*D*L^H.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /** @brief Returns copied provider/build/ABI identity.
   * @return Reference valid for this metadata object's lifetime.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }

 private:
  ReferencePositiveDefiniteTridiagonalFactorView(
      DenseBlasVectorView<const DenseBlasRealType<Element>> diagonal,
      DenseBlasVectorView<const Element> off_diagonal,
      DenseBlasTriangle triangle, LapackProviderIdentity provider)
      : diagonal_(diagonal),
        off_diagonal_(off_diagonal),
        triangle_(triangle),
        provider_(provider) {}
  DenseBlasVectorView<const DenseBlasRealType<Element>> diagonal_;
  DenseBlasVectorView<const Element> off_diagonal_;
  DenseBlasTriangle triangle_;
  LapackProviderIdentity provider_;
};

/** @brief Owns an explicit copy of finite positive PT factors on a host
 * resource.
 *
 * CopyFrom is an allocating convenience operation, separate from
 * allocation-free expert execution. It retains n real D and max(n-1,0) scalar E
 * entries using DenseArray's object-lifetime policy. Copying between
 * upper/lower orientations conjugates complex E and preserves the represented
 * matrix. No native call, factorization, scaling or new historical success
 * certificate occurs. The resource must outlive the owner. Immutable views may
 * be shared between solves with disjoint outputs; destruction or move
 * assignment invalidates views into the replaced storage. Moving construction
 * transfers their lifetime to the destination. A moved-from owner cannot
 * produce a view.
 */
template <DenseBlasScalar Element>
class ReferencePositiveDefiniteTridiagonalFactor {
 public:
  /** @brief Copies validated values into explicit host-owned storage.
   * @param provider Provider matching the borrowed factor identity.
   * @param factor Borrowed finite positive D and finite E; remains unchanged.
   * @param triangle Requested physical orientation of the owned copy.
   * @param resource Host allocator, required to outlive the returned owner.
   * @return Owner or validation/allocation failure. O(n) work and storage;
   * at most two allocations, no hidden workspace or provider execution.
   */
  static ASC_DENSE_LAPACK_EXPORT
      Result<ReferencePositiveDefiniteTridiagonalFactor>
      CopyFrom(const ReferenceLapackProvider& provider,
               ReferencePositiveDefiniteTridiagonalFactorView<Element> factor,
               DenseBlasTriangle triangle, MemoryResource& resource);
  /** @brief Transfers storage ownership without numerical work or allocation.
   */
  ReferencePositiveDefiniteTridiagonalFactor(
      ReferencePositiveDefiniteTridiagonalFactor&&) noexcept = default;
  /** @brief Releases old storage and transfers ownership from the source.
   * @return This owner; prior views into its old storage are invalidated.
   */
  ReferencePositiveDefiniteTridiagonalFactor& operator=(
      ReferencePositiveDefiniteTridiagonalFactor&&) noexcept = default;
  /** @brief Ownership is move-only; use CopyFrom for an explicit resource copy.
   */
  ReferencePositiveDefiniteTridiagonalFactor(
      const ReferencePositiveDefiniteTridiagonalFactor&) = delete;
  /** @brief Ownership is move-only; implicit resource copies are unavailable.
   */
  ReferencePositiveDefiniteTridiagonalFactor& operator=(
      const ReferencePositiveDefiniteTridiagonalFactor&) = delete;
  /** @brief Releases both arrays through the original live resource. */
  ~ReferencePositiveDefiniteTridiagonalFactor() = default;
  /** @brief Borrows immutable factors for repeated checked expert solves.
   * @param provider Must match the retained identity; no native call occurs.
   * @return Borrowed view, or invalid-state after move/provider mismatch.
   */
  [[nodiscard]] ASC_DENSE_LAPACK_EXPORT
      Result<ReferencePositiveDefiniteTridiagonalFactorView<Element>>
      view(const ReferenceLapackProvider& provider) const;

 private:
  using Shape = Extents<kDynamicExtent>;
  using Diagonal = DenseArray<DenseBlasRealType<Element>, Shape>;
  using OffDiagonal = DenseArray<Element, Shape>;
  ReferencePositiveDefiniteTridiagonalFactor(Diagonal diagonal,
                                             OffDiagonal off_diagonal,
                                             DenseBlasTriangle triangle,
                                             LapackProviderIdentity provider)
      : diagonal_(std::move(diagonal)),
        off_diagonal_(std::move(off_diagonal)),
        triangle_(triangle),
        provider_(provider) {}
  Diagonal diagonal_;
  OffDiagonal off_diagonal_;
  DenseBlasTriangle triangle_;
  LapackProviderIdentity provider_;
};

/** @brief Queries the zero-native-scratch PTTRF metadata contract.
 * @param provider Explicit admitted reference provider.
 * @param matrix Real D and lower scalar E, both unread by this query.
 * @return Matching zero-workspace plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<float> matrix);
/** @brief Computes the actual lower unit-bidiagonal/real-diagonal PT factor.
 * @param provider Explicit provider; no fallback or hidden scaling.
 * @param matrix D/E overwritten with factors or documented partial output.
 * @param plan Unmodified matching query result.
 * @param workspace Empty required scratch; all supplied regions validated.
 * @param report Surviving exact INFO, outcome and zero-based bad-pivot index.
 * @return OK, preflight/provider failure or nonpositive-factor kNumerical.
 * See file-level mutation, lifetime, cost and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pttrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteTridiagonalView<float> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Queries explicit layout scratch for a reused tridiagonal solve.
 * @param provider Explicit provider matching factor provenance.
 * @param factor Nominal immutable D/E and orientation; values are unread.
 * @param rhs Mutable n-by-nrhs B, either layout, values unread.
 * @return Matching checked plan; zero native scratch, no native entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
    DenseBlasMatrixView<float> rhs);
/** @brief Solves A*X=B with the actual PTTRS and immutable typed factors.
 * @param provider Same selected provider/build/ABI as factor and plan.
 * @param factor Finite positive D and finite oriented E, checked when active.
 * @param rhs B overwritten by X; padding and input factors are preserved.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint live scalar row-major packing if required.
 * @param report Surviving native INFO and qualified output validity.
 * @return OK or structural/numerical/provider failure, as described above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pttrs(const ReferenceLapackProvider& provider,
      ReferencePositiveDefiniteTridiagonalFactorView<float> factor,
      DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the zero-native-scratch PTTRF metadata contract.
 * @param provider Explicit admitted reference provider.
 * @param matrix Real D and lower scalar E, both unread by this query.
 * @return Matching zero-workspace plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<double> matrix);
/** @brief Computes the actual lower unit-bidiagonal/real-diagonal PT factor.
 * @param provider Explicit provider; no fallback or hidden scaling.
 * @param matrix D/E overwritten with factors or documented partial output.
 * @param plan Unmodified matching query result.
 * @param workspace Empty required scratch; all supplied regions validated.
 * @param report Surviving exact INFO, outcome and zero-based bad-pivot index.
 * @return OK, preflight/provider failure or nonpositive-factor kNumerical.
 * See file-level mutation, lifetime, cost and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pttrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteTridiagonalView<double> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Queries explicit layout scratch for a reused tridiagonal solve.
 * @param provider Explicit provider matching factor provenance.
 * @param factor Nominal immutable D/E and orientation; values are unread.
 * @param rhs Mutable n-by-nrhs B, either layout, values unread.
 * @return Matching checked plan; zero native scratch, no native entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
    DenseBlasMatrixView<double> rhs);
/** @brief Solves A*X=B with the actual PTTRS and immutable typed factors.
 * @param provider Same selected provider/build/ABI as factor and plan.
 * @param factor Finite positive D and finite oriented E, checked when active.
 * @param rhs B overwritten by X; padding and input factors are preserved.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint live scalar row-major packing if required.
 * @param report Surviving native INFO and qualified output validity.
 * @return OK or structural/numerical/provider failure, as described above.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pttrs(const ReferenceLapackProvider& provider,
      ReferencePositiveDefiniteTridiagonalFactorView<double> factor,
      DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries the zero-native-scratch PTTRF metadata contract.
 * @param provider Explicit admitted reference provider.
 * @param matrix Real D and lower scalar E, both unread by this query.
 * @return Matching zero-workspace plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix);
/** @brief Computes the actual lower unit-bidiagonal/real-diagonal PT factor.
 * @param provider Explicit provider; no fallback or hidden scaling.
 * @param matrix D/E overwritten with factors or documented partial output.
 * @param plan Unmodified matching query result.
 * @param workspace Empty required scratch; all supplied regions validated.
 * @param report Surviving exact INFO, outcome and zero-based bad-pivot index.
 * @return OK, preflight/provider failure or nonpositive-factor kNumerical.
 * See file-level mutation, lifetime, cost and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pttrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteTridiagonalView<std::complex<float>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Queries explicit layout scratch for a reused tridiagonal solve.
 * @param provider Explicit provider matching factor provenance.
 * @param factor Nominal immutable D/E and orientation; values are unread.
 * @param rhs Mutable n-by-nrhs B, either layout, values unread.
 * @return Matching checked plan; zero native scratch, no native entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs);
/** @brief Solves A*X=B with the actual PTTRS and immutable typed factors.
 * @param provider Same selected provider/build/ABI as factor and plan.
 * @param factor Finite positive D and finite oriented E, checked when active.
 * @param rhs B overwritten by X; padding and input factors are preserved.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint live scalar row-major packing if required.
 * @param report Surviving native INFO and qualified output validity.
 * @return OK or structural/numerical/provider failure, as described above.
 */
ASC_DENSE_LAPACK_EXPORT Status Pttrs(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

/** @brief Queries the zero-native-scratch PTTRF metadata contract.
 * @param provider Explicit admitted reference provider.
 * @param matrix Real D and lower scalar E, both unread by this query.
 * @return Matching zero-workspace plan or structural error; no native call.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix);
/** @brief Computes the actual lower unit-bidiagonal/real-diagonal PT factor.
 * @param provider Explicit provider; no fallback or hidden scaling.
 * @param matrix D/E overwritten with factors or documented partial output.
 * @param plan Unmodified matching query result.
 * @param workspace Empty required scratch; all supplied regions validated.
 * @param report Surviving exact INFO, outcome and zero-based bad-pivot index.
 * @return OK, preflight/provider failure or nonpositive-factor kNumerical.
 * See file-level mutation, lifetime, cost and concurrency semantics.
 */
ASC_DENSE_LAPACK_EXPORT Status
Pttrf(const ReferenceLapackProvider& provider,
      LapackPositiveDefiniteTridiagonalView<std::complex<double>> matrix,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);
/** @brief Queries explicit layout scratch for a reused tridiagonal solve.
 * @param provider Explicit provider matching factor provenance.
 * @param factor Nominal immutable D/E and orientation; values are unread.
 * @param rhs Mutable n-by-nrhs B, either layout, values unread.
 * @return Matching checked plan; zero native scratch, no native entry.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryPttrsWorkspace(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs);
/** @brief Solves A*X=B with the actual PTTRS and immutable typed factors.
 * @param provider Same selected provider/build/ABI as factor and plan.
 * @param factor Finite positive D and finite oriented E, checked when active.
 * @param rhs B overwritten by X; padding and input factors are preserved.
 * @param plan Unmodified matching metadata-only query result.
 * @param workspace Disjoint live scalar row-major packing if required.
 * @param report Surviving native INFO and qualified output validity.
 * @return OK or structural/numerical/provider failure, as described above.
 */
ASC_DENSE_LAPACK_EXPORT Status Pttrs(
    const ReferenceLapackProvider& provider,
    ReferencePositiveDefiniteTridiagonalFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs,
    const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
    LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_POSITIVE_TRIDIAGONAL_H_
