#ifndef ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_H_
#define ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_H_

/** @file
 * @brief Explicit rook Bunch–Kaufman symmetric and Hermitian factor/solve.
 *
 * SYTRF_ROOK/SYTF2_ROOK compute A=U*D*U^T or L*D*L^T, including complex
 * symmetric A. HETRF_ROOK/HETF2_ROOK compute A=U*D*U^H or L*D*L^H. U/L include
 * the documented permutations; D has 1-by-1 and 2-by-2 blocks. A raw pivot
 * vector is not a permutation. Each adjacent negative pivot records its own
 * interchange; the two values need not be equal. Classic factors are not
 * accepted by this nominal type.
 *
 * Operations are synchronous serial-CPU calls of the explicitly selected
 * optional provider. They allocate nothing, transfer nothing, change no
 * global state and never fall back to a different algorithm/provider.
 * Independent concurrent calls require disjoint writable buffers/reports.
 * Caller-owned factors, pivots, typed scalar workspace and metadata must stay
 * live and unmodified for their documented use. Nonempty storage must be
 * accessible to the explicit context; no device or managed execution is added.
 *
 * Every active operation requires n signed provider-width kInteger
 * entries. Execution starts these trivial private integer lifetimes in caller
 * byte storage. Scalar and layout regions already contain live T objects.
 * SYTRF_ROOK/HETRF_ROOK formula queries derive minimum WORK=1 and preferred
 * WORK from the pinned ILAENV NB=64 and its actual S/C rounding. Execution uses
 * the lesser of supplied scalar capacity and preferred capacity, retaining the
 * upstream blocked/reduced-block/unblocked selection.
 * SYTF2_ROOK/HETF2_ROOK/SYTRS_ROOK/HETRS_ROOK need no scalar WORK. Empty
 * operations need no workspace and make no foreign call. Query paths inspect
 * metadata, not matrix/pivot entries, and do not call LAPACK.
 *
 * Row-major factors use n*n kLayoutConversion entries. Original Hermitian
 * factorization inputs ALWAYS use those entries, even column-major, because
 * upstream panel/update paths otherwise copy ignored imaginary diagonals.
 * Packing reads only the selected offdiagonals and real original diagonals.
 * Symmetric complex reads full selected coefficients. Factor-consuming packing
 * preserves full block coefficients; it never normalizes them as original
 * Hermitian input. Row-major RHS adds n*nrhs layout entries. Only selected
 * factor output and logical RHS output are published; padding is unchanged.
 *
 * Plans bind actual scalar-prefixed routine, shape, triangle, symmetry,
 * original ASC strides, effective foreign strides, layouts and provider/ABI.
 * An unused single-column stride is normalized to the effective row count;
 * the original ASC stride remains part of the plan key.
 * Complete plan requirements, aliases, placement, paired-pivot bounds and
 * source INTEGER arithmetic are revalidated before any numerical write.
 * All mutable operands and scratch are disjoint from immutable inputs and live
 * metadata. Structural failures preserve numerical buffers with INFO absent
 * and called_provider=false. Any call-metadata alias rejection preserves the
 * report; otherwise it is reset before the remaining preflight checks.
 *
 * Factorization INFO>0 retains completed raw selected factors and checked raw
 * pivots, but never certifies a reusable successful factor. The pinned GNU
 * profile also reports scalar NaN using positive INFO through its MAX test;
 * the source has no general finiteness check. No finiteness or conditioning
 * guarantee follows from a successful return.
 * Solve checks exact zero 1-by-1 and 2-by-2 divisors before mutation; a
 * rejected numeric factor has no fabricated native INFO. Unexpected INFO is a
 * provider defect with the exact signed value retained. An unwritten full-width
 * INFO is likewise a provider defect; its signed sentinel is retained and
 * output is unusable. Raw source behavior and partial-output details are
 * described below.
 */

#include <array>
#include <complex>
#include <string_view>

#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/factor_view.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"
#include "asc/dense/providers/lapack_export.h"
#include "asc/dense/providers/lapack_indefinite.h"

namespace asc {

/** @brief Borrows a same-provider successful rook block factor and pivots.
 *
 * This optional provider-specific type is not a generic LDL factor or a
 * positive-definiteness certificate. It retains exact selected triangle,
 * symmetry, originating routine and provider identity. Mutation/expiration of
 * either borrowed buffer invalidates every consumer. Metadata cannot prove
 * content provenance: the caller guarantees buffers and report came from one
 * operation. No ownership, allocation, transfer or provider lifetime extension
 * occurs. Concurrent immutable reuse with distinct RHS/workspace is allowed.
 */
template <DenseBlasScalar Element>
class ReferenceRookFactorView {
 public:
  /** @brief Checks successful same-scalar report, placement and block pivots.
   * @param provider Explicit provider that produced the reported factor.
   * @param factors Same-call square selected factor; numerical entries unread.
   * @param triangle Same-call selected upper/lower triangle.
   * @param symmetry Same-call symmetric or Hermitian operation; no inference.
   * @param pivots Same-call immutable rook signed paired pivots, exact n.
   * @param report Successful complete same-call
   * SYTRF_ROOK/SYTF2_ROOK/HETRF_ROOK/HETF2_ROOK report.
   * @return Borrowed factor or structural/state error without numerical writes.
   * @pre Caller guarantees common origin and excludes concurrent mutation.
   */
  static ASC_DENSE_LAPACK_EXPORT Result<ReferenceRookFactorView> Create(
      const ReferenceLapackProvider& provider,
      DenseBlasMatrixView<const Element> factors, DenseBlasTriangle triangle,
      LapackBunchKaufmanSymmetry symmetry, RawLapackPivotView pivots,
      const LapackReport& report);

  /** @brief Returns borrowed selected block-factor storage.
   * @return Full descriptor retaining original layout, strides and lifetime.
   */
  [[nodiscard]] DenseBlasMatrixView<const Element> factors() const noexcept {
    return factors_;
  }
  /** @brief Returns borrowed rook signed paired pivots.
   * @return Immutable raw pivot view, never a normalized permutation.
   */
  [[nodiscard]] RawLapackPivotView pivots() const noexcept { return pivots_; }
  /** @brief Returns the actual factor triangle.
   * @return Selected upper/lower triangle, not inferred from values.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /** @brief Returns the factor's mathematical symmetry.
   * @return Transpose-based symmetric or conjugate-transpose Hermitian mode.
   */
  [[nodiscard]] LapackBunchKaufmanSymmetry symmetry() const noexcept {
    return symmetry_;
  }
  /** @brief Returns copied exact provider provenance.
   * @return Borrowed identity reference, valid for this metadata object's life.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }
  /** @brief Returns the copied actual scalar-prefixed source routine.
   * @return Borrowed text whose lifetime is this metadata object's lifetime.
   */
  [[nodiscard]] std::string_view originating_routine() const noexcept {
    return origin_.data();
  }

 private:
  ReferenceRookFactorView(DenseBlasMatrixView<const Element> factors,
                          RawLapackPivotView pivots, DenseBlasTriangle triangle,
                          LapackBunchKaufmanSymmetry symmetry,
                          const LapackReport& report)
      : factors_(factors),
        pivots_(pivots),
        triangle_(triangle),
        symmetry_(symmetry),
        provider_(report.provider),
        origin_(report.routine) {}

  DenseBlasMatrixView<const Element> factors_;
  RawLapackPivotView pivots_;
  DenseBlasTriangle triangle_;
  LapackBunchKaufmanSymmetry symmetry_;
  LapackProviderIdentity provider_;
  std::array<char, 32> origin_;
};

/** @brief Queries single real SYTRF_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual single real SYTRF_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<float> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYTRF_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual double real SYTRF_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<double> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex SYTRF_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual single complex SYTRF_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<float>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex SYTRF_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrfRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual double complex SYTRF_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrfRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<double>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single real SYTF2_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<float> matrix, DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual single real SYTF2_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytf2Rook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<float> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYTF2_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<double> matrix, DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual double real SYTF2_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytf2Rook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<double> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex SYTF2_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual single complex SYTF2_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytf2Rook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<float>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex SYTF2_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower symmetric input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytf2RookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual double complex SYTF2_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower symmetric input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
Sytf2Rook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<double>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex HETRF_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrfRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual single complex HETRF_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrfRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<float>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex HETRF_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrfRookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual double complex HETRF_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrfRook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<double>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex HETF2_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetf2RookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<float>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual single complex HETF2_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetf2Rook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<float>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double complex HETF2_ROOK workspace without reads.
 * @param provider Explicit checked reference provider; unchanged.
 * @param triangle Selected upper/lower Hermitian input triangle.
 * @param matrix Square input/output descriptor; entries are not read here.
 * @param pivots Disjoint contiguous signed ASC output, exact n; entries unread.
 * @return Complete formula-bound workspace plan or structural failure.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetf2RookWorkspace(
    const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
    DenseBlasMatrixView<std::complex<double>> matrix,
    DenseBlasVectorView<index_t> pivots);

/** @brief Executes actual double complex HETF2_ROOK Bunch–Kaufman.
 * @param provider Explicit same-build reference selection; no fallback.
 * @param triangle Selected upper/lower Hermitian input/output triangle.
 * @param matrix Square input, replaced by completed raw selected block factors.
 * @param pivots Disjoint contiguous n-entry signed one-based block output.
 * @param plan Unmodified matching query plan, fully checked before mutation.
 * @param workspace Explicit disjoint scalar/pivot/layout storage and lifetimes.
 * @param report Mandatory raw INFO/outcome/provenance and output-validity
 * report.
 * @return OK or structural/numerical/provider error. Positive INFO retains
 * completed raw factors and checked pivots as kDocumentedPartial, not a usable
 * successful factor. Invalid native pivots/INFO are never published as valid.
 */
ASC_DENSE_LAPACK_EXPORT Status
Hetf2Rook(const ReferenceLapackProvider& provider, DenseBlasTriangle triangle,
          DenseBlasMatrixView<std::complex<double>> matrix,
          DenseBlasVectorView<index_t> pivots, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single real SYTRS_ROOK workspace from metadata.
 * @param provider Explicit same-build reference selection.
 * @param factor Successful symmetric factor and immutable paired pivots.
 * @param rhs Disjoint n-by-nrhs input/output descriptor in either layout.
 * @return Bound pivot/layout formula plan or structural error, without reads
 * of numerical factors/pivots/RHS or foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsRookWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceRookFactorView<float>& factor,
    DenseBlasMatrixView<float> rhs);

/** @brief Solves single real symmetric A*X=B using SYTRS_ROOK.
 * @param provider Explicit provider matching the factor's copied identity.
 * @param factor Borrowed successful unchanged rook symmetric factor.
 * @param rhs Disjoint n-by-nrhs B, overwritten by X in the original layout.
 * @param plan Complete unchanged matching formula plan.
 * @param workspace Caller-owned disjoint pivot/layout conversion storage.
 * @param report Mandatory raw INFO and failure-surviving numerical report.
 * @return OK, preflight failure or provider defect. Incoming paired pivots and
 * exact zero block divisors are rechecked before writes. Unexpected native
 * INFO makes RHS unusable; factor/pivots remain unchanged. Success is not a
 * finiteness/conditioning certificate. Zero n/nrhs makes no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsRook(const ReferenceLapackProvider& provider,
          const ReferenceRookFactorView<float>& factor,
          DenseBlasMatrixView<float> rhs, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries double real SYTRS_ROOK workspace from metadata.
 * @param provider Explicit same-build reference selection.
 * @param factor Successful symmetric factor and immutable paired pivots.
 * @param rhs Disjoint n-by-nrhs input/output descriptor in either layout.
 * @return Bound pivot/layout formula plan or structural error, without reads
 * of numerical factors/pivots/RHS or foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsRookWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceRookFactorView<double>& factor,
    DenseBlasMatrixView<double> rhs);

/** @brief Solves double real symmetric A*X=B using SYTRS_ROOK.
 * @param provider Explicit provider matching the factor's copied identity.
 * @param factor Borrowed successful unchanged rook symmetric factor.
 * @param rhs Disjoint n-by-nrhs B, overwritten by X in the original layout.
 * @param plan Complete unchanged matching formula plan.
 * @param workspace Caller-owned disjoint pivot/layout conversion storage.
 * @param report Mandatory raw INFO and failure-surviving numerical report.
 * @return OK, preflight failure or provider defect. Incoming paired pivots and
 * exact zero block divisors are rechecked before writes. Unexpected native
 * INFO makes RHS unusable; factor/pivots remain unchanged. Success is not a
 * finiteness/conditioning certificate. Zero n/nrhs makes no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsRook(const ReferenceLapackProvider& provider,
          const ReferenceRookFactorView<double>& factor,
          DenseBlasMatrixView<double> rhs, const LapackWorkspacePlan& plan,
          const LapackWorkspace& workspace, LapackReport& report);

/** @brief Queries single complex SYTRS_ROOK workspace from metadata.
 * @param provider Explicit same-build reference selection.
 * @param factor Successful symmetric factor and immutable paired pivots.
 * @param rhs Disjoint n-by-nrhs input/output descriptor in either layout.
 * @return Bound pivot/layout formula plan or structural error, without reads
 * of numerical factors/pivots/RHS or foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsRookWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceRookFactorView<std::complex<float>>& factor,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Solves single complex symmetric A*X=B using SYTRS_ROOK.
 * @param provider Explicit provider matching the factor's copied identity.
 * @param factor Borrowed successful unchanged rook symmetric factor.
 * @param rhs Disjoint n-by-nrhs B, overwritten by X in the original layout.
 * @param plan Complete unchanged matching formula plan.
 * @param workspace Caller-owned disjoint pivot/layout conversion storage.
 * @param report Mandatory raw INFO and failure-surviving numerical report.
 * @return OK, preflight failure or provider defect. Incoming paired pivots and
 * exact zero block divisors are rechecked before writes. Unexpected native
 * INFO makes RHS unusable; factor/pivots remain unchanged. Success is not a
 * finiteness/conditioning certificate. Zero n/nrhs makes no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsRook(const ReferenceLapackProvider& provider,
          const ReferenceRookFactorView<std::complex<float>>& factor,
          DenseBlasMatrixView<std::complex<float>> rhs,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries double complex SYTRS_ROOK workspace from metadata.
 * @param provider Explicit same-build reference selection.
 * @param factor Successful symmetric factor and immutable paired pivots.
 * @param rhs Disjoint n-by-nrhs input/output descriptor in either layout.
 * @return Bound pivot/layout formula plan or structural error, without reads
 * of numerical factors/pivots/RHS or foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QuerySytrsRookWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceRookFactorView<std::complex<double>>& factor,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Solves double complex symmetric A*X=B using SYTRS_ROOK.
 * @param provider Explicit provider matching the factor's copied identity.
 * @param factor Borrowed successful unchanged rook symmetric factor.
 * @param rhs Disjoint n-by-nrhs B, overwritten by X in the original layout.
 * @param plan Complete unchanged matching formula plan.
 * @param workspace Caller-owned disjoint pivot/layout conversion storage.
 * @param report Mandatory raw INFO and failure-surviving numerical report.
 * @return OK, preflight failure or provider defect. Incoming paired pivots and
 * exact zero block divisors are rechecked before writes. Unexpected native
 * INFO makes RHS unusable; factor/pivots remain unchanged. Success is not a
 * finiteness/conditioning certificate. Zero n/nrhs makes no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Status
SytrsRook(const ReferenceLapackProvider& provider,
          const ReferenceRookFactorView<std::complex<double>>& factor,
          DenseBlasMatrixView<std::complex<double>> rhs,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries single complex HETRS_ROOK workspace from metadata.
 * @param provider Explicit same-build reference selection.
 * @param factor Successful Hermitian factor and immutable paired pivots.
 * @param rhs Disjoint n-by-nrhs input/output descriptor in either layout.
 * @return Bound pivot/layout formula plan or structural error, without reads
 * of numerical factors/pivots/RHS or foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrsRookWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceRookFactorView<std::complex<float>>& factor,
    DenseBlasMatrixView<std::complex<float>> rhs);

/** @brief Solves single complex Hermitian A*X=B using HETRS_ROOK.
 * @param provider Explicit provider matching the factor's copied identity.
 * @param factor Borrowed successful unchanged rook Hermitian factor.
 * @param rhs Disjoint n-by-nrhs B, overwritten by X in the original layout.
 * @param plan Complete unchanged matching formula plan.
 * @param workspace Caller-owned disjoint pivot/layout conversion storage.
 * @param report Mandatory raw INFO and failure-surviving numerical report.
 * @return OK, preflight failure or provider defect. Incoming paired pivots and
 * exact zero block divisors are rechecked before writes. Unexpected native
 * INFO makes RHS unusable; factor/pivots remain unchanged. Success is not a
 * finiteness/conditioning certificate. Zero n/nrhs makes no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrsRook(const ReferenceLapackProvider& provider,
          const ReferenceRookFactorView<std::complex<float>>& factor,
          DenseBlasMatrixView<std::complex<float>> rhs,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

/** @brief Queries double complex HETRS_ROOK workspace from metadata.
 * @param provider Explicit same-build reference selection.
 * @param factor Successful Hermitian factor and immutable paired pivots.
 * @param rhs Disjoint n-by-nrhs input/output descriptor in either layout.
 * @return Bound pivot/layout formula plan or structural error, without reads
 * of numerical factors/pivots/RHS or foreign calls.
 */
ASC_DENSE_LAPACK_EXPORT Result<LapackWorkspacePlan> QueryHetrsRookWorkspace(
    const ReferenceLapackProvider& provider,
    const ReferenceRookFactorView<std::complex<double>>& factor,
    DenseBlasMatrixView<std::complex<double>> rhs);

/** @brief Solves double complex Hermitian A*X=B using HETRS_ROOK.
 * @param provider Explicit provider matching the factor's copied identity.
 * @param factor Borrowed successful unchanged rook Hermitian factor.
 * @param rhs Disjoint n-by-nrhs B, overwritten by X in the original layout.
 * @param plan Complete unchanged matching formula plan.
 * @param workspace Caller-owned disjoint pivot/layout conversion storage.
 * @param report Mandatory raw INFO and failure-surviving numerical report.
 * @return OK, preflight failure or provider defect. Incoming paired pivots and
 * exact zero block divisors are rechecked before writes. Unexpected native
 * INFO makes RHS unusable; factor/pivots remain unchanged. Success is not a
 * finiteness/conditioning certificate. Zero n/nrhs makes no foreign call.
 */
ASC_DENSE_LAPACK_EXPORT Status
HetrsRook(const ReferenceLapackProvider& provider,
          const ReferenceRookFactorView<std::complex<double>>& factor,
          DenseBlasMatrixView<std::complex<double>> rhs,
          const LapackWorkspacePlan& plan, const LapackWorkspace& workspace,
          LapackReport& report);

}  // namespace asc

#endif  // ASC_DENSE_PROVIDERS_LAPACK_INDEFINITE_ROOK_H_
