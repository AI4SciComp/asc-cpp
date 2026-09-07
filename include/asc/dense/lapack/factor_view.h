#ifndef ASC_DENSE_LAPACK_FACTOR_VIEW_H_
#define ASC_DENSE_LAPACK_FACTOR_VIEW_H_

/** @file
 * @brief Borrowed, family-tagged raw pivots and successful native LU factors.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/dense/blas.h"
#include "asc/dense/export.h"
#include "asc/dense/lapack/report.h"
#include "asc/dense/lapack/structured_view.h"
#include "asc/dense/lapack/types.h"

namespace asc {

/** @brief Borrows signed one-based raw pivots, preserving family conventions.
 *
 * Entries are already widened to ASC signed 64-bit storage. No foreign 32-bit
 * array may be reinterpreted as this type. The owner and its immutable elements
 * must outlive every view/consumer. Negative block encodings are preserved;
 * only the separately named LU conversion interprets sequential row swaps.
 */
class RawLapackPivotView {
 public:
  /** @brief Checks contiguous backing bounds, alignment, placement and family.
   * @param values Contiguous signed raw pivot entries; null only when empty.
   * @param size Number of entries, not bytes; must be nonnegative.
   * @param family Exact factor algorithm and pivot encoding.
   * @param backing Complete host/pinned-host backing span and lifetime promise.
   * @return Borrowed view or validation failure. No entries are modified/read.
   */
  static ASC_DENSE_EXPORT Result<RawLapackPivotView> Create(
      const index_t* values, extent_t size, LapackFactorFamily family,
      ConstMemoryView backing);
  /** @brief Returns raw signed entries; never changes one-based/block encoding.
   * @return Raw signed entries; never changes one-based/block encoding.
   */
  [[nodiscard]] std::span<const index_t> values() const noexcept {
    return {values_.data(), static_cast<std::size_t>(values_.size())};
  }
  /** @brief Returns the originating algorithm's pivot convention.
   * @return The originating algorithm's pivot convention.
   */
  [[nodiscard]] LapackFactorFamily family() const noexcept { return family_; }
  /** @brief Returns the exact reachable raw-pivot bytes for alias preflight.
   * @return The exact reachable raw-pivot bytes for alias preflight.
   */
  [[nodiscard]] ConstMemoryView reachable_storage() const noexcept {
    return values_.reachable_storage();
  }

 private:
  RawLapackPivotView(DenseBlasVectorView<const index_t> values,
                     LapackFactorFamily family)
      : values_(values), family_(family) {}
  DenseBlasVectorView<const index_t> values_;
  LapackFactorFamily family_;
};

/** @brief Validates each GETRF pivot p[i] in [i+1, rows], without writes.
 * @param pivots Borrowed raw GETRF sequential pivots, not a permutation.
 * @param rows Original factor row count; pivot length must not exceed it.
 * @return OK or invalid argument/index, including rejection of block families.
 * CPU-only, allocation-free; the backing entries must remain live and
 * immutable.
 */
ASC_DENSE_EXPORT Status ValidateLuPivots(RawLapackPivotView pivots,
                                         extent_t rows);

/** @brief Converts GETRF one-based entries into zero-based sequential swaps.
 * @param pivots Raw LU partial-pivot payload, already in signed 64-bit storage.
 * @param rows Original factor row count.
 * @param destination Caller-owned contiguous zero-based swap output, exact
 * size.
 * @return OK or validation failure. All bounds and overlap checks precede
 * writes; source and destination must be disjoint. No allocation/transfer
 * occurs. The result is a swap sequence, not a final permutation; block tags
 * are rejected.
 */
ASC_DENSE_EXPORT Status ConvertLuPivotsToZeroBasedSwaps(
    RawLapackPivotView pivots, extent_t rows, std::span<index_t> destination);

/** @brief Borrows a successful LU factor and its validated pivot provenance.
 *
 * Uses the existing full-matrix BLAS descriptor without packing or allocation.
 * Mutation/destruction of factors or pivots invalidates the borrowed factor.
 * A partial/singular result remains available in raw buffers, but cannot be
 * created as this successful reusable factor. Concurrent reads are permitted;
 * concurrent mutation is forbidden. CPU-only; no provider fallback is implied.
 */
template <DenseBlasScalar Element>
class LapackLuFactorView {
 public:
  /** @brief Validates shape, pivot length/encoding and successful report
   * family.
   * @param factors Packed L/U from the reported successful GETRF execution.
   * @param pivots Same execution's raw sequential pivot buffer.
   * @param report Successful complete LU report; copied provenance is retained.
   * @return Borrowed factor or invalid state/shape/index; no output is
   * modified.
   * @pre Caller guarantees the report and buffers came from the same call.
   */
  static Result<LapackLuFactorView> Create(
      DenseBlasMatrixView<const Element> factors, RawLapackPivotView pivots,
      const LapackReport& report) {
    if (report.outcome != LapackOutcome::kSuccess ||
        report.output_validity != LapackOutputValidity::kComplete ||
        report.factor_family != LapackFactorFamily::kLuPartialPivot) {
      return Status(ErrorCode::kInvalidState);
    }
    if (factors.memory_space() != MemorySpace::kHost &&
        factors.memory_space() != MemorySpace::kPinnedHost) {
      return Status(ErrorCode::kMemoryAccess);
    }
    if (pivots.values().size() !=
        static_cast<std::size_t>(std::min(factors.rows(), factors.columns()))) {
      return Status(ErrorCode::kShape);
    }
    const ConstMemoryView matrix_storage = factors.reachable_storage();
    const ConstMemoryView pivot_storage = pivots.reachable_storage();
    const auto matrix_begin =
        reinterpret_cast<std::uintptr_t>(matrix_storage.data());
    const auto pivot_begin =
        reinterpret_cast<std::uintptr_t>(pivot_storage.data());
    if (matrix_storage.size() != 0 && pivot_storage.size() != 0 &&
        matrix_begin < pivot_begin + pivot_storage.size() &&
        pivot_begin < matrix_begin + matrix_storage.size()) {
      return Status(ErrorCode::kInvalidArgument);
    }
    const Status status = ValidateLuPivots(pivots, factors.rows());
    if (!status.ok()) {
      return status;
    }
    return LapackLuFactorView(factors, pivots, report.provider);
  }
  /** @brief Returns borrowed packed factors, retaining padding and layout.
   * @return Borrowed packed factors, retaining padding and layout.
   */
  [[nodiscard]] DenseBlasMatrixView<const Element> factors() const noexcept {
    return factors_;
  }
  /** @brief Returns the corresponding immutable raw sequential pivot view.
   * @return The corresponding immutable raw sequential pivot view.
   */
  [[nodiscard]] RawLapackPivotView pivots() const noexcept { return pivots_; }
  /** @brief Returns copied provenance; it does not extend provider lifetime.
   * @return Copied provenance; it does not extend provider lifetime.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }

 private:
  LapackLuFactorView(DenseBlasMatrixView<const Element> factors,
                     RawLapackPivotView pivots, LapackProviderIdentity provider)
      : factors_(factors), pivots_(pivots), provider_(provider) {}
  DenseBlasMatrixView<const Element> factors_;
  RawLapackPivotView pivots_;
  LapackProviderIdentity provider_;
};

/** @brief Borrows a complete POTRF factor with explicit selected triangle.
 *
 * The owner and unmodified factor values must outlive consumers. Lower stores
 * A=L*L^H and upper stores A=U^H*U for complex elements; real uses transpose.
 * Ignored triangle/padding are not read. This is CPU metadata, with no packing,
 * value verification, allocation or provider lifetime extension on success.
 */
template <DenseBlasScalar Element>
class LapackCholeskyFactorView {
 public:
  /** @brief Validates square CPU storage, selected triangle and successful
   * family.
   * @param factors Borrowed completed triangular factor in a full matrix view.
   * @param triangle Triangle actually computed by the originating operation.
   * @param report The same operation's complete successful Cholesky report.
   * @return Borrowed factor or shape/invalid-state/tag failure, without writes.
   * @pre Caller guarantees the report, triangle and buffer came from one call.
   */
  static Result<LapackCholeskyFactorView> Create(
      DenseBlasMatrixView<const Element> factors, DenseBlasTriangle triangle,
      const LapackReport& report) {
    if (!internal_dense_lapack::IsTriangle(triangle) ||
        !internal_dense_lapack::IsHost(factors.memory_space())) {
      return Status(ErrorCode::kInvalidArgument);
    }
    if (factors.rows() != factors.columns()) {
      return Status(ErrorCode::kShape);
    }
    if (report.outcome != LapackOutcome::kSuccess ||
        report.output_validity != LapackOutputValidity::kComplete ||
        report.factor_family != LapackFactorFamily::kCholesky) {
      return Status(ErrorCode::kInvalidState);
    }
    return LapackCholeskyFactorView(factors, triangle, report.provider);
  }
  /** @brief Returns borrowed triangular coefficients in their full descriptor.
   * @return Borrowed triangular coefficients in their full descriptor.
   */
  [[nodiscard]] DenseBlasMatrixView<const Element> factors() const noexcept {
    return factors_;
  }
  /** @brief Returns the selected upper/lower factor triangle.
   * @return The selected upper/lower factor triangle.
   */
  [[nodiscard]] DenseBlasTriangle triangle() const noexcept {
    return triangle_;
  }
  /** @brief Returns copied provider provenance, not an owned context.
   * @return Copied provider provenance, not an owned context.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }

 private:
  LapackCholeskyFactorView(DenseBlasMatrixView<const Element> factors,
                           DenseBlasTriangle triangle,
                           LapackProviderIdentity provider)
      : factors_(factors), triangle_(triangle), provider_(provider) {}
  DenseBlasMatrixView<const Element> factors_;
  DenseBlasTriangle triangle_;
  LapackProviderIdentity provider_;
};

/** @brief Borrows a complete GEQRF packed matrix and its separate tau entries.
 *
 * Describes min(m,n) elementary reflectors and upper trapezoidal R without
 * forming Q. Full/reduced Q formation or application is a separately explicit
 * operation. Mutation/destruction of either buffer invalidates all consumers;
 * no rank certificate or provider availability is implied by this metadata.
 */
template <DenseBlasScalar Element>
class LapackHouseholderQrFactorView {
 public:
  /** @brief Checks reflector count, disjoint contiguous tau and successful
   * family.
   * @param factors Borrowed packed GEQRF reflectors/R; tall, square or wide.
   * @param tau Same-call contiguous min(m,n) scalar reflector coefficients.
   * @param report Same-call complete successful Householder QR report.
   * @return Borrowed factor or state/shape/alias failure. CPU-only; no reads of
   * numerical entries, writes, hidden packing or allocation occur on success.
   */
  static Result<LapackHouseholderQrFactorView> Create(
      DenseBlasMatrixView<const Element> factors,
      DenseBlasVectorView<const Element> tau, const LapackReport& report) {
    if (report.outcome != LapackOutcome::kSuccess ||
        report.output_validity != LapackOutputValidity::kComplete ||
        report.factor_family != LapackFactorFamily::kHouseholderQr) {
      return Status(ErrorCode::kInvalidState);
    }
    if (tau.size() != std::min(factors.rows(), factors.columns()) ||
        !internal_dense_lapack::Contiguous(tau)) {
      return Status(ErrorCode::kShape);
    }
    const std::array<ConstMemoryView, 2> spans{factors.reachable_storage(),
                                               tau.reachable_storage()};
    Status status = internal_dense_lapack::Disjoint(spans);
    if (!status.ok()) {
      return status;
    }
    return LapackHouseholderQrFactorView(factors, tau, report.provider);
  }
  /** @brief Returns the borrowed packed reflector/R matrix.
   * @return The borrowed packed reflector/R matrix.
   */
  [[nodiscard]] DenseBlasMatrixView<const Element> factors() const noexcept {
    return factors_;
  }
  /** @brief Returns borrowed scalar reflector coefficients.
   * @return Borrowed scalar reflector coefficients.
   */
  [[nodiscard]] DenseBlasVectorView<const Element> tau() const noexcept {
    return tau_;
  }
  /** @brief Returns copied origin metadata; does not own a provider.
   * @return Copied origin metadata; does not own a provider.
   */
  [[nodiscard]] const LapackProviderIdentity& provider() const noexcept {
    return provider_;
  }

 private:
  LapackHouseholderQrFactorView(DenseBlasMatrixView<const Element> factors,
                                DenseBlasVectorView<const Element> tau,
                                LapackProviderIdentity provider)
      : factors_(factors), tau_(tau), provider_(provider) {}
  DenseBlasMatrixView<const Element> factors_;
  DenseBlasVectorView<const Element> tau_;
  LapackProviderIdentity provider_;
};

}  // namespace asc

#endif  // ASC_DENSE_LAPACK_FACTOR_VIEW_H_
