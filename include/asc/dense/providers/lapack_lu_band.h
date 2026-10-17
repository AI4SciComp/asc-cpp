#ifndef ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_H_
#define ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_H_

/** @file
 * @brief Explicit pinned Reference-LAPACK general-band LU factorization/solve.
 *
 * GBTRF and explicit unblocked GBTF2 overwrite column-major LapackLuBandView
 * storage with the exact upstream band factors. Original A(i,j) occupies row
 * kl+ku+i-j (zero-based); the first kl storage rows provide additional U fill,
 * never input coefficients. U has kl+ku superdiagonals. The multipliers below
 * its diagonal combine with interleaved row swaps; these are not a dense GETRF
 * L and permutation. Signed ASC pivots preserve the positive one-based
 * GBTRF/GBTF2 encoding: step j (zero-based) swaps rows j and pivots[j]-1, where
 * j+1 <= pivots[j] <= min(m,j+kl+1). No generic GETRF factor tag or permutation
 * is substituted. The distinct nominal factor type below carries the family;
 * factor_family remains absent in reports. Positive INFO preserves the source's
 * completed raw band factorization and all checked pivots as documented partial
 * output, but cannot create a successful reusable factor.
 *
 * The band descriptor is column-major only, including required fill capacity.
 * Its ld padding remains untouched; source-defined fill initialization may
 * overwrite reserved fill corners. GBTRS independently accepts either RHS
 * layout and N, T, or C. It overwrites only logical B with X in op(A)*X=B.
 * Factor/pivot storage is immutable and reusable. No densification, inverse,
 * refactorization, tolerance, or blanket finiteness scan is performed.
 *
 * Queries inspect metadata only and make no foreign call. None of these
 * routines has a scalar WORK query. Active factorization needs min(m,n)
 * kInteger entries; active solve needs n. Entry size/alignment is the actual
 * selected signed provider integer ABI. Execution starts those trivial integer
 * lifetimes in caller storage. A nonempty row-major RHS additionally requires
 * n*nrhs live scalar objects in kLayoutConversion; other scalar regions are
 * unused. The upstream GBTRF chooses its pinned blocked/unblocked algorithm
 * and has fixed local work arrays, independently audited in the review. GBTF2
 * directly executes unblocked elimination and has no local work array. Its
 * plan identity is distinct; GBTRF and GBTF2 plans cannot be interchanged.
 *
 * Exact plan requirements/identity, native source integer intermediates,
 * dimensions, placement, aliases and capacities are checked before numerical
 * mutation. Caller buffers, metadata and scratch must remain live and disjoint.
 * The explicit serial CPU context must access host/pinned-host operands;
 * there is no allocation, transfer, synchronization or provider substitution
 * in ASC. Provider allocation evidence is scoped separately in the review.
 * Independent calls use disjoint writable storage and reports. Borrowed
 * buffers are not owned and provider lifetimes are not extended.
 *
 * Reports retain exact signed INFO, actual routine and provider. Structural
 * rejection has absent INFO, called_provider=false and unchanged numerical
 * storage. An unsafe report/metadata alias is rejected without resetting it;
 * other attempts reset the report before preflight. Positive factor INFO=i
 * returns kNumerical/kSingular, zero-based diagnostic i-1 and documented
 * partial output. Negative/impossible/unwritten INFO or invalid returned pivots
 * returns kProvider/kUnusable; directly supplied band/RHS mutation cannot be
 * rolled back, while packed RHS and output pivots are not published on such
 * defects. GBTRS preflight rejects an exact zero U diagonal without inventing
 * INFO; its provider has no numerical positive INFO. A successful INFO is not a
 * finite-result or conditioning guarantee. Valid empty calls use safe dummy
 * arguments for the actual source quick return, retaining its real INFO.
 */

#include <array>
#include <complex>
#include <string_view>

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

/** @brief Borrows one successful GBTRF/GBTF2 band factorization and raw swaps.
 *
 * Rectangular factors are admitted for inspection; GBTRS requires a square
 * factor. The caller guarantees buffers and report share one GBTRF or GBTF2
 * origin, with the same shape and bandwidths. Metadata cannot authenticate
 * contents. Mutation or expiration invalidates this view and every consumer.
 * Immutable reuse with independent RHS/workspace is allowed. No ownership is
 * acquired.
 */
template <DenseBlasScalar Element>
class ReferenceLuBandFactorView {
 public:
  /** @brief Checks provider, exact successful report, placement and all swaps.
   * @param provider Explicit provider which produced this factorization.
   * @param factors Same-call const band storage including U fill.
   * @param pivots Same-call contiguous signed one-based GBTRF/GBTF2 swaps,
   * min(m,n).
   * @param report Complete successful same-scalar GBTRF/GBTF2 report with INFO
   * zero.
   * @return Borrowed nominal factor or failure without numerical mutation.
   * @pre Caller guarantees common origin, lifetime and immutable contents.
   */
  static ASC_DENSE_LAPACK_EXPORT Result<ReferenceLuBandFactorView> Create(
      const ReferenceLapackProvider& provider,
      LapackLuBandView<const Element> factors,
      DenseBlasVectorView<const index_t> pivots, const LapackReport& report);

  /** @brief Returns borrowed immutable factors, shape, bandwidths and stride.
   * @return Original descriptor; this does not extend its storage lifetime.
   */
  [[nodiscard]] LapackLuBandView<const Element> factors() const noexcept {
    return factors_;
  }
  /** @brief Returns immutable raw GBTRF/GBTF2 swaps, not a permutation.
   * @return Signed one-based values with the original borrowed lifetime.
   */
  [[nodiscard]] DenseBlasVectorView<const index_t> pivots() const noexcept {
    return pivots_;
  }
  /** @brief Returns exact copied provider and integer-ABI provenance.
   * @return Identity reference valid for this metadata object's lifetime.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }
  /** @brief Returns copied actual scalar-prefixed GBTRF or GBTF2 origin.
   * @return Text borrowed from this metadata object's lifetime.
   */
  [[nodiscard]] std::string_view originating_routine() const noexcept {
    return origin_.data();
  }

 private:
  ReferenceLuBandFactorView(LapackLuBandView<const Element> factors,
                            DenseBlasVectorView<const index_t> pivots,
                            const LapackReport& report)
      : factors_(factors),
        pivots_(pivots),
        provider_(report.provider),
        origin_(report.routine) {}

  LapackLuBandView<const Element> factors_;
  DenseBlasVectorView<const index_t> pivots_;
  LapackProviderIdentity provider_;
  std::array<char, 32> origin_;
};

/** @brief Queries single real GBTRF integer conversion storage without reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable band descriptor with all required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return Routine/scalar/shape/bandwidth/stride/provider-bound checked plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<float> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual SGBTRF, preserving source band fill and swaps.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Band input/output; ld padding remains unchanged.
 * @param pivots Contiguous signed one-based output, exactly min(m,n).
 * @param plan Exact unchanged QueryGbtrfWorkspace result.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO/provenance report.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbtrf(const ReferenceLapackProvider& provider,
                                     LapackLuBandView<float> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single real GBTRS integer/RHS conversion storage.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C mathematical operation; complex C conjugates.
 * @param factor Nominal immutable successful square GBTRF/GBTF2 factor and
 * swaps.
 * @param rhs Mutable n-by-nrhs B/X in either independent layout.
 * @return Exact checked formula plan without reading numerical/pivot values.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<float> factor, DenseBlasMatrixView<float> rhs);

/** @brief Executes actual SGBTRS using a reusable immutable band factor.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C in op(A)*X=B, without changing precision.
 * @param factor Successful square GBTRF/GBTF2 factor; all pivots are rechecked.
 * @param rhs Mutable B/X; only logical entries are published.
 * @param plan Exact unchanged QueryGbtrsWorkspace result.
 * @param workspace Disjoint native-integer and explicit RHS packing storage.
 * @param report Mandatory raw INFO/result report, accessible on every failure.
 * @return OK, structural/numeric preflight failure, or unexpected kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbtrs(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose operation,
                                     ReferenceLuBandFactorView<float> factor,
                                     DenseBlasMatrixView<float> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real GBTRF integer conversion storage without reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable band descriptor with all required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return Routine/scalar/shape/bandwidth/stride/provider-bound checked plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<double> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual DGBTRF, preserving source band fill and swaps.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Band input/output; ld padding remains unchanged.
 * @param pivots Contiguous signed one-based output, exactly min(m,n).
 * @param plan Exact unchanged QueryGbtrfWorkspace result.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO/provenance report.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbtrf(const ReferenceLapackProvider& provider,
                                     LapackLuBandView<double> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real GBTRS integer/RHS conversion storage.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C mathematical operation; complex C conjugates.
 * @param factor Nominal immutable successful square GBTRF/GBTF2 factor and
 * swaps.
 * @param rhs Mutable n-by-nrhs B/X in either independent layout.
 * @return Exact checked formula plan without reading numerical/pivot values.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<double> factor, DenseBlasMatrixView<double> rhs);

/** @brief Executes actual DGBTRS using a reusable immutable band factor.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C in op(A)*X=B, without changing precision.
 * @param factor Successful square GBTRF/GBTF2 factor; all pivots are rechecked.
 * @param rhs Mutable B/X; only logical entries are published.
 * @param plan Exact unchanged QueryGbtrsWorkspace result.
 * @param workspace Disjoint native-integer and explicit RHS packing storage.
 * @param report Mandatory raw INFO/result report, accessible on every failure.
 * @return OK, structural/numeric preflight failure, or unexpected kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbtrs(const ReferenceLapackProvider& provider,
                                     DenseBlasTranspose operation,
                                     ReferenceLuBandFactorView<double> factor,
                                     DenseBlasMatrixView<double> rhs,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single complex GBTRF integer conversion storage without
 * reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable band descriptor with all required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return Routine/scalar/shape/bandwidth/stride/provider-bound checked plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual CGBTRF, preserving source band fill and swaps.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Band input/output; ld padding remains unchanged.
 * @param pivots Contiguous signed one-based output, exactly min(m,n).
 * @param plan Exact unchanged QueryGbtrfWorkspace result.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO/provenance report.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbtrf(const ReferenceLapackProvider& provider,
      LapackLuBandView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex GBTRS integer/RHS conversion storage.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C mathematical operation; complex C conjugates.
 * @param factor Nominal immutable successful square GBTRF/GBTF2 factor and
 * swaps.
 * @param rhs Mutable n-by-nrhs B/X in either independent layout.
 * @return Exact checked formula plan without reading numerical/pivot values.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<std::complex<float>> factor,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Executes actual CGBTRS using a reusable immutable band factor.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C in op(A)*X=B, without changing precision.
 * @param factor Successful square GBTRF/GBTF2 factor; all pivots are rechecked.
 * @param rhs Mutable B/X; only logical entries are published.
 * @param plan Exact unchanged QueryGbtrsWorkspace result.
 * @param workspace Disjoint native-integer and explicit RHS packing storage.
 * @param report Mandatory raw INFO/result report, accessible on every failure.
 * @return OK, structural/numeric preflight failure, or unexpected kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbtrs(const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
      ReferenceLuBandFactorView<std::complex<float>> factor,
      DenseBlasMatrixView<std::complex<float>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries double complex GBTRF integer conversion storage without
 * reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable band descriptor with all required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return Routine/scalar/shape/bandwidth/stride/provider-bound checked plan.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrfWorkspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual ZGBTRF, preserving source band fill and swaps.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Band input/output; ld padding remains unchanged.
 * @param pivots Contiguous signed one-based output, exactly min(m,n).
 * @param plan Exact unchanged QueryGbtrfWorkspace result.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO/provenance report.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbtrf(const ReferenceLapackProvider& provider,
      LapackLuBandView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex GBTRS integer/RHS conversion storage.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C mathematical operation; complex C conjugates.
 * @param factor Nominal immutable successful square GBTRF/GBTF2 factor and
 * swaps.
 * @param rhs Mutable n-by-nrhs B/X in either independent layout.
 * @return Exact checked formula plan without reading numerical/pivot values.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtrsWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
    ReferenceLuBandFactorView<std::complex<double>> factor,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Executes actual ZGBTRS using a reusable immutable band factor.
 * @param provider Same explicit provider that produced the factors.
 * @param operation N, T, or C in op(A)*X=B, without changing precision.
 * @param factor Successful square GBTRF/GBTF2 factor; all pivots are rechecked.
 * @param rhs Mutable B/X; only logical entries are published.
 * @param plan Exact unchanged QueryGbtrsWorkspace result.
 * @param workspace Disjoint native-integer and explicit RHS packing storage.
 * @param report Mandatory raw INFO/result report, accessible on every failure.
 * @return OK, structural/numeric preflight failure, or unexpected kProvider.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbtrs(const ReferenceLapackProvider& provider, DenseBlasTranspose operation,
      ReferenceLuBandFactorView<std::complex<double>> factor,
      DenseBlasMatrixView<std::complex<double>> rhs,
      const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
      LapackReport& report);

/** @brief Queries single real GBTF2 integer conversion storage without reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable m-by-n band descriptor with required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return GBTF2-specific scalar/shape/bandwidth/stride/provider-bound plan.
 * @details No scalar WORK or provider workspace query exists. The plan requires
 * min(m,n) native integer entries, including output-pivot conversion in LP64.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtf2Workspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<float> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual unblocked SGBTF2 with partial row pivoting.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix General band A overwritten by U and interleaved multipliers.
 * @param pivots Signed one-based step swaps, exactly min(m,n) output entries.
 * @param plan Exact unchanged QueryGbtf2Workspace result for these operands.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO and actual provenance.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 * @details Positive INFO=i preserves completed raw factors and checked pivots
 * as documented partial output, with zero-based diagnostic i-1. Only a complete
 * successful report can create a ReferenceLuBandFactorView for GBTRS. Complex
 * entries describe a general matrix; there is no Hermitian or UPLO assumption.
 * @pre Operands, metadata, report and workspace satisfy the file-level lifetime
 * and disjointness contract. INFO zero does not guarantee finite factors.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbtf2(const ReferenceLapackProvider& provider,
                                     LapackLuBandView<float> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries double real GBTF2 integer conversion storage without reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable m-by-n band descriptor with required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return GBTF2-specific scalar/shape/bandwidth/stride/provider-bound plan.
 * @details No scalar WORK or provider workspace query exists. The plan requires
 * min(m,n) native integer entries, including output-pivot conversion in LP64.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtf2Workspace(
    const ReferenceLapackProvider& provider, LapackLuBandView<double> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual unblocked DGBTF2 with partial row pivoting.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix General band A overwritten by U and interleaved multipliers.
 * @param pivots Signed one-based step swaps, exactly min(m,n) output entries.
 * @param plan Exact unchanged QueryGbtf2Workspace result for these operands.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO and actual provenance.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 * @details Positive INFO=i preserves completed raw factors and checked pivots
 * as documented partial output, with zero-based diagnostic i-1. Only a complete
 * successful report can create a ReferenceLuBandFactorView for GBTRS. Complex
 * entries describe a general matrix; there is no Hermitian or UPLO assumption.
 * @pre Operands, metadata, report and workspace satisfy the file-level lifetime
 * and disjointness contract. INFO zero does not guarantee finite factors.
 */
ASC_DENSE_LAPACK_EXPORT Status Gbtf2(const ReferenceLapackProvider& provider,
                                     LapackLuBandView<double> matrix,
                                     DenseBlasVectorView<index_t> pivots,
                                     const LapackWorkspacePlan& plan,
                                     const LapackWorkspace& workspace,
                                     LapackReport& report);

/** @brief Queries single complex GBTF2 integer conversion storage without
 * reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable m-by-n band descriptor with required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return GBTF2-specific scalar/shape/bandwidth/stride/provider-bound plan.
 * @details No scalar WORK or provider workspace query exists. The plan requires
 * min(m,n) native integer entries, including output-pivot conversion in LP64.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual unblocked CGBTF2 with partial row pivoting.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix General band A overwritten by U and interleaved multipliers.
 * @param pivots Signed one-based step swaps, exactly min(m,n) output entries.
 * @param plan Exact unchanged QueryGbtf2Workspace result for these operands.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO and actual provenance.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 * @details Positive INFO=i preserves completed raw factors and checked pivots
 * as documented partial output, with zero-based diagnostic i-1. Only a complete
 * successful report can create a ReferenceLuBandFactorView for GBTRS. Complex
 * entries describe a general matrix; there is no Hermitian or UPLO assumption.
 * @pre Operands, metadata, report and workspace satisfy the file-level lifetime
 * and disjointness contract. INFO zero does not guarantee finite factors.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbtf2(const ReferenceLapackProvider& provider,
      LapackLuBandView<std::complex<float>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex GBTF2 integer conversion storage without
 * reads.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix Mutable m-by-n band descriptor with required U fill capacity.
 * @param pivots Contiguous signed output with exactly min(m,n) entries.
 * @return GBTF2-specific scalar/shape/bandwidth/stride/provider-bound plan.
 * @details No scalar WORK or provider workspace query exists. The plan requires
 * min(m,n) native integer entries, including output-pivot conversion in LP64.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryGbtf2Workspace(
    const ReferenceLapackProvider& provider,
    LapackLuBandView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual unblocked ZGBTF2 with partial row pivoting.
 * @param provider Explicit pinned serial CPU provider.
 * @param matrix General band A overwritten by U and interleaved multipliers.
 * @param pivots Signed one-based step swaps, exactly min(m,n) output entries.
 * @param plan Exact unchanged QueryGbtf2Workspace result for these operands.
 * @param workspace Disjoint live storage meeting every typed requirement.
 * @param report Mandatory failure-surviving raw INFO and actual provenance.
 * @return OK, preflight failure, kNumerical for exact singularity, or
 * kProvider.
 * @details Positive INFO=i preserves completed raw factors and checked pivots
 * as documented partial output, with zero-based diagnostic i-1. Only a complete
 * successful report can create a ReferenceLuBandFactorView for GBTRS. Complex
 * entries describe a general matrix; there is no Hermitian or UPLO assumption.
 * @pre Operands, metadata, report and workspace satisfy the file-level lifetime
 * and disjointness contract. INFO zero does not guarantee finite factors.
 */
ASC_DENSE_LAPACK_EXPORT Status
Gbtf2(const ReferenceLapackProvider& provider,
      LapackLuBandView<std::complex<double>> matrix,
      DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
      const LapackWorkspace& workspace, LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_LU_BAND_H_
