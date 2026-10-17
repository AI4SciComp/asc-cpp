#ifndef ASC_DENSE_PROVIDERS_LAPACK_COMPLETE_PIVOTING_H_
#define ASC_DENSE_PROVIDERS_LAPACK_COMPLETE_PIVOTING_H_

/** @file
 * @brief Checked complete-pivoting LU factors and explicitly scaled solves.
 *
 * GETC2 stores unit-lower L and upper U plus independent one-based sequential
 * row/column swaps. Applying both forward swap sequences to the original
 * matrix gives L*U, except for documented small-pivot perturbations. Positive
 * INFO identifies the last perturbed pivot: execution succeeds with an
 * accuracy warning and complete factors for the perturbed system. The named
 * factor view below retains this distinction; GETRF factor views cannot accept
 * its reports. factor_family remains absent; exact GETC2 routine provenance
 * identifies this separate complete-pivoting protocol.
 *
 * GESC2 solves one RHS and publishes the explicit real SCALE in [0,1] with
 * A_factor*X=SCALE*B. SCALE is never silently divided out. Perturbed factors or
 * SCALE<1 retain an accuracy warning. Native GESC2 has no INFO argument; the
 * report never invents one. Empty execution is local, with SCALE=1 for solves.
 *
 * Input matrix/factor/RHS components must be finite. Nonfinite input returns
 * kNumerical before workspace writes or native entry. Finite arithmetic can
 * still overflow; nonfinite results return kNumerical without publication.
 * Successful factors/solutions do not imply good conditioning. No tolerance,
 * scaling fallback or replacement factor algorithm is introduced.
 *
 * Both matrix layouts use explicit caller-owned live scalar kScratch storage:
 * N*N for GETC2, N*N+N for GESC2. Two native-width pivot arrays use aligned
 * kInteger byte storage with 2*N entries; execution begins their lifetimes.
 * Empty order needs neither region. Public pivots use immutable/writable
 * contiguous signed index_t entries as appropriate, never foreign typedefs.
 * Queries inspect metadata only and bind routine/scalar/provider, order,
 * matrix layout/stride, vector sizes/increments and factor perturbation state.
 * A matching copied plan is reusable with disjoint equally shaped storage.
 *
 * Every numeric span, workspace region, provider, plan, workspace and report
 * object is disjoint. Metadata overlap returns kInvalidArgument without report
 * reset; other structural failures reset the report and preserve numeric
 * storage. Execution rechecks pivots before packing. Native INFO/pivot/scale
 * defects withhold staged output; INFO zero or valid returned metadata does
 * not certify arbitrary omitted/wrong numerical writes. Padding stays intact.
 * Calls require accessible host/pinned-host storage and serial CPU execution,
 * allocate/transfer/synchronize nothing, and are reentrant with disjoint work.
 */

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"

namespace asc {

/** @brief Borrows complete GETC2 factors and two distinct sequential pivots.
 *
 * A warning denotes the perturbed factor system, which may differ from the
 * original matrix. Storage remains caller-owned, immutable and live while the
 * view is used. Construction checks report provenance, shape, placement and
 * each pivot in [i+1,N], but does not recompute or certify numerical factors.
 * No GETRF, final-permutation, block-pivot or other factor view is accepted.
 */
template <DenseBlasScalar Element>
class ReferenceCompletePivotLuFactorView {
 public:
  /** @brief Validates a successful or perturbed complete-pivoting factor.
   * @param factors Square packed L/U from the same GETC2 call as the report.
   * @param row_pivots Contiguous exact-N one-based sequential row swaps.
   * @param column_pivots Disjoint exact-N one-based sequential column swaps.
   * @param report That GETC2 call's complete success/accuracy-warning report.
   * @return Borrowed factor or structural/index/state failure, without writes.
   * @pre Caller guarantees the report and all buffers came from one call;
   * the factor and pivot values must remain unchanged after construction.
   */
  static Result<ReferenceCompletePivotLuFactorView> Create(
      DenseBlasMatrixView<const Element> factors,
      DenseBlasVectorView<const index_t> row_pivots,
      DenseBlasVectorView<const index_t> column_pivots,
      const LapackReport& report) {
    const extent_t n = factors.rows();
    if (factors.columns() != n || row_pivots.size() != n ||
        column_pivots.size() != n || row_pivots.increment() != 1 ||
        column_pivots.increment() != 1) {
      return Status(ErrorCode::kShape);
    }
    if (!ValidReport(report, n)) {
      return Status(ErrorCode::kInvalidState);
    }
    const std::array spans{factors.reachable_storage(),
                           row_pivots.reachable_storage(),
                           column_pivots.reachable_storage()};
    for (std::size_t i = 0; i < spans.size(); ++i) {
      if (spans[i].space() != MemorySpace::kHost &&
          spans[i].space() != MemorySpace::kPinnedHost) {
        return Status(ErrorCode::kMemoryAccess);
      }
      for (std::size_t j = i + 1; j < spans.size(); ++j) {
        if (Overlap(spans[i], spans[j])) {
          return Status(ErrorCode::kInvalidArgument);
        }
      }
    }
    for (extent_t i = 0; i < n; ++i) {
      if (row_pivots.data()[i] < i + 1 || row_pivots.data()[i] > n ||
          column_pivots.data()[i] < i + 1 || column_pivots.data()[i] > n) {
        return Status(ErrorCode::kIndex);
      }
    }
    return ReferenceCompletePivotLuFactorView(
        factors, row_pivots, column_pivots, report.provider,
        report.native_info.value_or(0) > 0);
  }
  /** @brief Returns the immutable packed unit-L/upper-U factor descriptor.
   * @return Borrowed matrix retaining its original layout and leading stride.
   */
  [[nodiscard]] DenseBlasMatrixView<const Element> factors() const noexcept {
    return factors_;
  }
  /** @brief Returns the immutable one-based sequential row swaps.
   * @return Borrowed exact-N row pivot descriptor; not a final permutation.
   */
  [[nodiscard]] DenseBlasVectorView<const index_t> row_pivots() const noexcept {
    return row_pivots_;
  }
  /** @brief Returns the immutable one-based sequential column swaps.
   * @return Borrowed exact-N column pivot descriptor; not GETRF row pivots.
   */
  [[nodiscard]] DenseBlasVectorView<const index_t> column_pivots()
      const noexcept {
    return column_pivots_;
  }
  /** @brief Returns copied exact provider provenance for same-build solves.
   * @return Retained identity; no provider or buffer lifetime is extended.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }
  /** @brief Reports whether GETC2 replaced one or more small U pivots.
   * @return True for a perturbed system, not an estimate of conditioning.
   */
  [[nodiscard]] bool perturbed() const noexcept { return perturbed_; }

 private:
  static bool Overlap(ConstMemoryView a, ConstMemoryView b) {
    if (a.size() == 0 || b.size() == 0) {
      return false;
    }
    const auto x = reinterpret_cast<std::uintptr_t>(a.data());
    const auto y = reinterpret_cast<std::uintptr_t>(b.data());
    return x <= y ? y - x < a.size() : x - y < b.size();
  }
  static constexpr char Prefix() {
    if constexpr (std::is_same_v<std::remove_cv_t<Element>, float>) {
      return 's';
    } else if constexpr (std::is_same_v<std::remove_cv_t<Element>, double>) {
      return 'd';
    } else if constexpr (std::is_same_v<std::remove_cv_t<Element>,
                                        std::complex<float>>) {
      return 'c';
    } else {
      return 'z';
    }
  }
  static bool ValidReport(const LapackReport& report, extent_t n) {
    constexpr std::array<char, 32> kRoutine{Prefix(), 'g', 'e', 't', 'c', '2'};
    if (report.routine != kRoutine || report.factor_family.has_value() ||
        report.provider.kind != LapackProviderKind::kReference ||
        report.output_validity != LapackOutputValidity::kComplete ||
        report.native_argument.has_value()) {
      return false;
    }
    if (n == 0) {
      return !report.called_provider && !report.native_info.has_value() &&
             !report.diagnostic_index.has_value() &&
             report.outcome == LapackOutcome::kSuccess;
    }
    if (!report.called_provider || !report.native_info.has_value() ||
        *report.native_info < 0 || *report.native_info > n) {
      return false;
    }
    if (*report.native_info > 0) {
      if (report.diagnostic_index != *report.native_info - 1) {
        return false;
      }
    } else if (report.diagnostic_index.has_value()) {
      return false;
    }
    return report.outcome == (*report.native_info > 0
                                  ? LapackOutcome::kAccuracyWarning
                                  : LapackOutcome::kSuccess);
  }
  ReferenceCompletePivotLuFactorView(
      DenseBlasMatrixView<const Element> factors,
      DenseBlasVectorView<const index_t> row_pivots,
      DenseBlasVectorView<const index_t> column_pivots,
      LapackProviderIdentity provider, bool perturbed)
      : factors_(factors),
        row_pivots_(row_pivots),
        column_pivots_(column_pivots),
        provider_(provider),
        perturbed_(perturbed) {}
  DenseBlasMatrixView<const Element> factors_;
  DenseBlasVectorView<const index_t> row_pivots_;
  DenseBlasVectorView<const index_t> column_pivots_;
  LapackProviderIdentity provider_;
  bool perturbed_;
};

/** @brief Plans SGETC2 with no numeric reads or provider query.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param matrix Square matrix; row/column layout and padding are retained.
 * @param row_pivots Disjoint contiguous exact-N signed row-pivot output.
 * @param column_pivots Disjoint contiguous exact-N signed column-pivot output.
 * @return Matching checked scalar/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<float> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots);
/** @brief Factors through SGETC2, retaining usable pivot perturbations.
 * @param provider Explicit serial CPU Reference provider; no fallback.
 * @param matrix Finite square input, replaced by complete packed L/U.
 * @param row_pivots Exact-N output of one-based sequential row swaps.
 * @param column_pivots Exact-N output of one-based sequential column swaps.
 * @param plan Unmodified matching query plan, reusable with disjoint buffers.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Mandatory raw INFO, provenance and last perturbed index.
 * @return OK for complete factors, with kAccuracyWarning on positive INFO;
 * structural/numerical/provider errors withhold caller numeric publication.
 * Metadata overlap preserves the report; other checks reset it. See file.
 */
ASC_DENSE_LAPACK_EXPORT Status Getc2(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<float> matrix,
                                     DenseBlasVectorView<index_t> row_pivots,
                                     DenseBlasVectorView<index_t> column_pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans SGESC2 without reading numerical values.
 * @param provider Exact provider retained by the complete-pivot factor.
 * @param factor Borrowed complete GETC2 factors, possibly perturbed.
 * @param rhs Disjoint contiguous exact-N input/output vector.
 * @param scale Disjoint one-entry real output, also required when N=0.
 * @return Matching checked staging/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<float> factor,
    DenseBlasVectorView<float> rhs, DenseBlasVectorView<float> scale);
/** @brief Solves through SGESC2, preserving explicit SCALE.
 * @param provider Exact serial CPU Reference provider retained by factor.
 * @param factor Immutable finite complete factors with nonzero U diagonals.
 * @param rhs Finite exact-N vector B, overwritten by X on completion.
 * @param scale One real output in [0,1] with A_factor*X=SCALE*B; empty sets1.
 * @param plan Matching unmodified plan; factor perturbation state is bound.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Required diagnostics; INFO stays absent for this routine.
 * @return OK with kAccuracyWarning for perturbed factors or SCALE<1;
 * structural/numerical/provider errors preserve RHS and SCALE. Metadata aliases
 * preserve report; other failures reset it. No unscaling or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gesc2(const ReferenceLapackProvider& provider,
      ReferenceCompletePivotLuFactorView<float> factor,
      DenseBlasVectorView<float> rhs, DenseBlasVectorView<float> scale,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Plans DGETC2 with no numeric reads or provider query.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param matrix Square matrix; row/column layout and padding are retained.
 * @param row_pivots Disjoint contiguous exact-N signed row-pivot output.
 * @param column_pivots Disjoint contiguous exact-N signed column-pivot output.
 * @return Matching checked scalar/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider, DenseBlasMatrixView<double> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots);
/** @brief Factors through DGETC2, retaining usable pivot perturbations.
 * @param provider Explicit serial CPU Reference provider; no fallback.
 * @param matrix Finite square input, replaced by complete packed L/U.
 * @param row_pivots Exact-N output of one-based sequential row swaps.
 * @param column_pivots Exact-N output of one-based sequential column swaps.
 * @param plan Unmodified matching query plan, reusable with disjoint buffers.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Mandatory raw INFO, provenance and last perturbed index.
 * @return OK for complete factors, with kAccuracyWarning on positive INFO;
 * structural/numerical/provider errors withhold caller numeric publication.
 * Metadata overlap preserves the report; other checks reset it. See file.
 */
ASC_DENSE_LAPACK_EXPORT Status Getc2(const ReferenceLapackProvider& provider,
                                     DenseBlasMatrixView<double> matrix,
                                     DenseBlasVectorView<index_t> row_pivots,
                                     DenseBlasVectorView<index_t> column_pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);
/** @brief Plans DGESC2 without reading numerical values.
 * @param provider Exact provider retained by the complete-pivot factor.
 * @param factor Borrowed complete GETC2 factors, possibly perturbed.
 * @param rhs Disjoint contiguous exact-N input/output vector.
 * @param scale Disjoint one-entry real output, also required when N=0.
 * @return Matching checked staging/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<double> factor,
    DenseBlasVectorView<double> rhs, DenseBlasVectorView<double> scale);
/** @brief Solves through DGESC2, preserving explicit SCALE.
 * @param provider Exact serial CPU Reference provider retained by factor.
 * @param factor Immutable finite complete factors with nonzero U diagonals.
 * @param rhs Finite exact-N vector B, overwritten by X on completion.
 * @param scale One real output in [0,1] with A_factor*X=SCALE*B; empty sets1.
 * @param plan Matching unmodified plan; factor perturbation state is bound.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Required diagnostics; INFO stays absent for this routine.
 * @return OK with kAccuracyWarning for perturbed factors or SCALE<1;
 * structural/numerical/provider errors preserve RHS and SCALE. Metadata aliases
 * preserve report; other failures reset it. No unscaling or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gesc2(const ReferenceLapackProvider& provider,
      ReferenceCompletePivotLuFactorView<double> factor,
      DenseBlasVectorView<double> rhs, DenseBlasVectorView<double> scale,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Plans CGETC2 with no numeric reads or provider query.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param matrix Square matrix; row/column layout and padding are retained.
 * @param row_pivots Disjoint contiguous exact-N signed row-pivot output.
 * @param column_pivots Disjoint contiguous exact-N signed column-pivot output.
 * @return Matching checked scalar/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots);
/** @brief Factors through CGETC2, retaining usable pivot perturbations.
 * @param provider Explicit serial CPU Reference provider; no fallback.
 * @param matrix Finite square input, replaced by complete packed L/U.
 * @param row_pivots Exact-N output of one-based sequential row swaps.
 * @param column_pivots Exact-N output of one-based sequential column swaps.
 * @param plan Unmodified matching query plan, reusable with disjoint buffers.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Mandatory raw INFO, provenance and last perturbed index.
 * @return OK for complete factors, with kAccuracyWarning on positive INFO;
 * structural/numerical/provider errors withhold caller numeric publication.
 * Metadata overlap preserves the report; other checks reset it. See file.
 */
ASC_DENSE_LAPACK_EXPORT Status Getc2(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans CGESC2 without reading numerical values.
 * @param provider Exact provider retained by the complete-pivot factor.
 * @param factor Borrowed complete GETC2 factors, possibly perturbed.
 * @param rhs Disjoint contiguous exact-N input/output vector.
 * @param scale Disjoint one-entry real output, also required when N=0.
 * @return Matching checked staging/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<std::complex<float>> factor,
    DenseBlasVectorView<std::complex<float>> rhs,
    DenseBlasVectorView<float> scale);
/** @brief Solves through CGESC2, preserving explicit SCALE.
 * @param provider Exact serial CPU Reference provider retained by factor.
 * @param factor Immutable finite complete factors with nonzero U diagonals.
 * @param rhs Finite exact-N vector B, overwritten by X on completion.
 * @param scale One real output in [0,1] with A_factor*X=SCALE*B; empty sets1.
 * @param plan Matching unmodified plan; factor perturbation state is bound.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Required diagnostics; INFO stays absent for this routine.
 * @return OK with kAccuracyWarning for perturbed factors or SCALE<1;
 * structural/numerical/provider errors preserve RHS and SCALE. Metadata aliases
 * preserve report; other failures reset it. No unscaling or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gesc2(const ReferenceLapackProvider& provider,
      ReferenceCompletePivotLuFactorView<std::complex<float>> factor,
      DenseBlasVectorView<std::complex<float>> rhs,
      DenseBlasVectorView<float> scale, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Plans ZGETC2 with no numeric reads or provider query.
 * @param provider Explicit same-build serial CPU Reference provider.
 * @param matrix Square matrix; row/column layout and padding are retained.
 * @param row_pivots Disjoint contiguous exact-N signed row-pivot output.
 * @param column_pivots Disjoint contiguous exact-N signed column-pivot output.
 * @return Matching checked scalar/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGetc2Workspace(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots);
/** @brief Factors through ZGETC2, retaining usable pivot perturbations.
 * @param provider Explicit serial CPU Reference provider; no fallback.
 * @param matrix Finite square input, replaced by complete packed L/U.
 * @param row_pivots Exact-N output of one-based sequential row swaps.
 * @param column_pivots Exact-N output of one-based sequential column swaps.
 * @param plan Unmodified matching query plan, reusable with disjoint buffers.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Mandatory raw INFO, provenance and last perturbed index.
 * @return OK for complete factors, with kAccuracyWarning on positive INFO;
 * structural/numerical/provider errors withhold caller numeric publication.
 * Metadata overlap preserves the report; other checks reset it. See file.
 */
ASC_DENSE_LAPACK_EXPORT Status Getc2(
    const ReferenceLapackProvider& provider,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> row_pivots,
    DenseBlasVectorView<index_t> column_pivots, const LapackWorkspacePlan& plan,
    const LapackWorkspace& workspace, LapackReport& report);
/** @brief Plans ZGESC2 without reading numerical values.
 * @param provider Exact provider retained by the complete-pivot factor.
 * @param factor Borrowed complete GETC2 factors, possibly perturbed.
 * @param rhs Disjoint contiguous exact-N input/output vector.
 * @param scale Disjoint one-entry real output, also required when N=0.
 * @return Matching checked staging/integer plan or structural/overflow error.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGesc2Workspace(
    const ReferenceLapackProvider& provider,
    ReferenceCompletePivotLuFactorView<std::complex<double>> factor,
    DenseBlasVectorView<std::complex<double>> rhs,
    DenseBlasVectorView<double> scale);
/** @brief Solves through ZGESC2, preserving explicit SCALE.
 * @param provider Exact serial CPU Reference provider retained by factor.
 * @param factor Immutable finite complete factors with nonzero U diagonals.
 * @param rhs Finite exact-N vector B, overwritten by X on completion.
 * @param scale One real output in [0,1] with A_factor*X=SCALE*B; empty sets1.
 * @param plan Matching unmodified plan; factor perturbation state is bound.
 * @param workspace Disjoint live scalar staging and aligned integer bytes.
 * @param report Required diagnostics; INFO stays absent for this routine.
 * @return OK with kAccuracyWarning for perturbed factors or SCALE<1;
 * structural/numerical/provider errors preserve RHS and SCALE. Metadata aliases
 * preserve report; other failures reset it. No unscaling or allocation occurs.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gesc2(const ReferenceLapackProvider& provider,
      ReferenceCompletePivotLuFactorView<std::complex<double>> factor,
      DenseBlasVectorView<std::complex<double>> rhs,
      DenseBlasVectorView<double> scale, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_COMPLETE_PIVOTING_H_
